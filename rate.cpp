/* smushplay - A simple LucasArts SMUSH video player
 *
 * smushplay is the legal property of its developers, whose names can be
 * found in the AUTHORS file distributed with this source
 * distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * The code in this file is based on code with Copyright 1998 Fabrice Bellard
 * Fabrice original code is part of SoX (http://sox.sourceforge.net).
 * Max Horn adapted that code to the needs of ScummVM and rewrote it partial,
 * in the process removing any use of floating point arithmetic. Various other
 * improvements over the original code were made.
 */

// Based on the ScummVM code of the same name (GPLv2+)

#include <assert.h>
#include <stdio.h>
#include "audiostream.h"
#include "rate.h"

/**
 * The precision of the fractional (fixed point) type we define below.
 * Normally you should never have to modify this value.
 */
enum {
	FRAC_BITS = 16,
	FRAC_LO_MASK = ((1L << FRAC_BITS) - 1),
	FRAC_HI_MASK = ((1L << FRAC_BITS) - 1) << FRAC_BITS,

	FRAC_ONE = (1L << FRAC_BITS),		// 1.0
	FRAC_HALF = (1L << (FRAC_BITS - 1))	// 0.5
};

/**
 * Fixed-point fractions, used by the sound rate converter and other code.
 */
typedef int32 Fraction;

/* Minimum and maximum values a sample can hold. */
enum {
	ST_SAMPLE_MAX = 0x7fffL,
	ST_SAMPLE_MIN = (-ST_SAMPLE_MAX - 1L)
};

static void clampedAdd(int16& a, int b) {
	register int val = a + b;

	if (val > ST_SAMPLE_MAX)
		val = ST_SAMPLE_MAX;
	else if (val < ST_SAMPLE_MIN)
		val = ST_SAMPLE_MIN;

	a = val;
}

/**
 * The size of the intermediate input cache. Bigger values may increase
 * performance, but only until some point (depends largely on cache size,
 * target processor and various other factors), at which it will decrease
 * again.
 */
#define INTERMEDIATE_BUFFER_SIZE 512


/**
 * Audio rate converter based on simple resampling. Used when no
 * interpolation is required.
 */
template<bool stereo, bool reverseStereo>
class SimpleRateConverter : public RateConverter {
protected:
	int16 _inBuf[INTERMEDIATE_BUFFER_SIZE];
	const int16 *_inPtr;
	int _inLen;

	/** position of how far output is ahead of input */
	/** Holds what would have been _outPos-ipos */
	long _outPos;

	/** fractional position increment in the output stream */
	long _outPosInc;

public:
	SimpleRateConverter(uint32 inRate, uint32 outRate);
	int flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume);
};


/*
 * Prepare processing.
 */
template<bool stereo, bool reverseStereo>
SimpleRateConverter<stereo, reverseStereo>::SimpleRateConverter(uint32 inRate, uint32 outRate) {
	assert((inRate % outRate) == 0);

	_outPos = 1;

	/* increment */
	_outPosInc = inRate / outRate;

	_inLen = 0;
}

/*
 * Processed signed long samples from ibuf to outBuffer.
 * Return number of sample pairs processed.
 */
template<bool stereo, bool reverseStereo>
int SimpleRateConverter<stereo, reverseStereo>::flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume) {
	int16 *outStart = outBuffer;
	int16 *outEnd = outBuffer + outSamples * 2;

	while (outBuffer < outEnd) {
		// Read enough input samples so that _outPos >= 0
		do {
			// Check if we have to refill the buffer
			if (_inLen == 0) {
				_inPtr = _inBuf;
				_inLen = input.readBuffer(_inBuf, INTERMEDIATE_BUFFER_SIZE);
				if (_inLen <= 0)
					return (outBuffer - outStart) / 2;
			}
			_inLen -= (stereo ? 2 : 1);
			_outPos--;
			if (_outPos >= 0) {
				_inPtr += (stereo ? 2 : 1);
			}
		} while (_outPos >= 0);

		int16 out0, out1;
		out0 = *_inPtr++;
		out1 = (stereo ? *_inPtr++ : out0);

		// Increment output position
		_outPos += _outPosInc;

		// output left channel
		clampedAdd(outBuffer[reverseStereo], (out0 * (int)leftVolume) / 0x100);

		// output right channel
		clampedAdd(outBuffer[reverseStereo ^ 1], (out1 * (int)rightVolume) / 0x100);

		outBuffer += 2;
	}

	return (outBuffer - outStart) / 2;
}

/**
 * Audio rate converter based on simple linear Interpolation.
 *
 * The use of fractional increment allows us to use no buffer. It
 * avoid the problems at the end of the buffer we had with the old
 * method which stored a possibly big buffer of size
 * lcm(in_rate,out_rate).
 *
 * Limited to sampling frequency <= 65535 Hz.
 */

template<bool stereo, bool reverseStereo>
class LinearRateConverter : public RateConverter {
protected:
	int16 _inBuf[INTERMEDIATE_BUFFER_SIZE];
	const int16 *_inPtr;
	int _inLen;

	/** fractional position of the output stream in input stream unit */
	Fraction _outPos;

	/** fractional position increment in the output stream */
	Fraction _outPosInc;

	/** last sample(s) in the input stream (left/right channel) */
	int16 _inLast0, _inLast1;
	/** current sample(s) in the input stream (left/right channel) */
	int16 _inCur0, _inCur1;

public:
	LinearRateConverter(uint32 inRate, uint32 outRate);
	int flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume);
};


/*
 * Prepare processing.
 */
template<bool stereo, bool reverseStereo>
LinearRateConverter<stereo, reverseStereo>::LinearRateConverter(uint32 inRate, uint32 outRate) {
	assert(inRate < 65536 && outRate < 65536);

	_outPos = FRAC_ONE;

	// Compute the linear interpolation increment.
	// This will overflow if inrate >= 2^16, and underflow if outrate >= 2^16.
	// Also, if the quotient of the two rate becomes too small / too big, that
	// would cause problems, but since we rarely scale from 1 to 65536 Hz or vice
	// versa, I think we can live with that limitation ;-).
	_outPosInc = (inRate << FRAC_BITS) / outRate;

	_inLast0 = _inLast1 = 0;
	_inCur0 = _inCur1 = 0;

	_inLen = 0;
}

/*
 * Processed signed long samples from ibuf to outBuffer.
 * Return number of sample pairs processed.
 */
template<bool stereo, bool reverseStereo>
int LinearRateConverter<stereo, reverseStereo>::flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume) {
	int16 *outStart = outBuffer;
	int16 *outEnd = outBuffer + outSamples * 2;

	while (outBuffer < outEnd) {
		// Read enough input samples so that _outPos < 0
		while ((Fraction)FRAC_ONE <= _outPos) {
			// Check if we have to refill the buffer
			if (_inLen == 0) {
				_inPtr = _inBuf;
				_inLen = input.readBuffer(_inBuf, INTERMEDIATE_BUFFER_SIZE);
				if (_inLen <= 0)
					return (outBuffer - outStart) / 2;
			}
			_inLen -= (stereo ? 2 : 1);
			_inLast0 = _inCur0;
			_inCur0 = *_inPtr++;
			if (stereo) {
				_inLast1 = _inCur1;
				_inCur1 = *_inPtr++;
			}
			_outPos -= FRAC_ONE;
		}

		// Loop as long as the outpos trails behind, and as long as there is
		// still space in the output buffer.
		while (_outPos < (Fraction)FRAC_ONE && outBuffer < outEnd) {
			// interpolate
			int16 out0 = (int16)(_inLast0 + (((_inCur0 - _inLast0) * _outPos + FRAC_HALF) >> FRAC_BITS));
			int16 out1 = (stereo ? (int16)(_inLast1 + (((_inCur1 - _inLast1) * _outPos + FRAC_HALF) >> FRAC_BITS)) : out0);

			// output left channel
			clampedAdd(outBuffer[reverseStereo], (out0 * (int)leftVolume) / 0x100);

			// output right channel
			clampedAdd(outBuffer[reverseStereo ^ 1], (out1 * (int)rightVolume) / 0x100);

			outBuffer += 2;

			// Increment output position
			_outPos += _outPosInc;
		}
	}
	return (outBuffer - outStart) / 2;
}

/**
 * Simple audio rate converter for the case that the inRate equals the outRate.
 */
template<bool stereo, bool reverseStereo>
class CopyRateConverter : public RateConverter {
	int16 *_buffer;
	uint32 _bufferSize;
public:
	CopyRateConverter() : _buffer(0), _bufferSize(0) {}
	~CopyRateConverter() {
		delete[] _buffer;
	}

	virtual int flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume) {
		assert((input.getChannels() == 2) == stereo);

		if (stereo)
			outSamples *= 2;

		// Reallocate temp buffer, if necessary
		if (outSamples > _bufferSize) {
			delete[] _buffer;
			_buffer = new int16[outSamples];
			_bufferSize = outSamples;
		}

		// Read up to 'outSamples' samples into our temporary buffer
		uint32 len = input.readBuffer(_buffer, outSamples);

		int16 *outStart = outBuffer;

		// Mix the data into the output buffer
		int16 *ptr = _buffer;
		for (; len > 0; len -= (stereo ? 2 : 1)) {
			int16 out0, out1;
			out0 = *ptr++;
			out1 = (stereo ? *ptr++ : out0);

			// output left channel
			clampedAdd(outBuffer[reverseStereo], (out0 * (int)leftVolume) / 0x100);

			// output right channel
			clampedAdd(outBuffer[reverseStereo ^ 1], (out1 * (int)rightVolume) / 0x100);

			outBuffer += 2;
		}

		return (outBuffer - outStart) / 2;
	}
};

template<bool stereo, bool reverseStereo>
RateConverter *makeRateConverter(uint32 inRate, uint32 outRate) {
	if (inRate != outRate) {
		if ((inRate % outRate) == 0) {
			return new SimpleRateConverter<stereo, reverseStereo>(inRate, outRate);
		} else {
			return new LinearRateConverter<stereo, reverseStereo>(inRate, outRate);
		}
	} else {
		return new CopyRateConverter<stereo, reverseStereo>();
	}
}

/**
 * Create and return a RateConverter object for the specified input and output rates.
 */
RateConverter *makeRateConverter(uint32 inRate, uint32 outRate, bool stereo, bool reverseStereo) {
	if (stereo) {
		if (reverseStereo)
			return makeRateConverter<true, true>(inRate, outRate);
		else
			return makeRateConverter<true, false>(inRate, outRate);
	} else
		return makeRateConverter<false, false>(inRate, outRate);
}
