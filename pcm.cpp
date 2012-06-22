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

// Based on the xoreos code of the same name, which is in turn
// based on the RawStream code of ScummVM (GPLv3+ and GPLv2+,
// respectively).

#include <string.h> // for size_t
#include "audiostream.h"
#include "pcm.h"
#include "util.h"

// This used to be an inline template function, but
// buggy template function handling in MSVC6 forced
// us to go with the macro approach. So far this is
// the only template function that MSVC6 seemed to
// compile incorrectly. Knock on wood.
#define READ_ENDIAN_SAMPLE(is16Bit, isUnsigned, isLE) \
	((is16Bit ? (isLE ? READ_LE_UINT16(_curSample) : READ_BE_UINT16(_curSample)) : (*_curSample << 8)) ^ (isUnsigned ? 0x8000 : 0))


/**
 * This is a stream, which allows for playing raw PCM data from a stream.
 * It also features playback of multiple blocks from a given stream.
 */
template<bool is16Bit, bool isUnsigned, bool isLE>
class PCMStream : public AudioStream {

protected:
	const int _rate;                     ///< Sample rate of stream
	const int _channels;                 ///< Amount of channels

	byte *_data, *_curSample;
	uint32 _size;
	const bool _disposeAfterUse;         ///< Indicates whether the stream object should be deleted when this RawStream is destructed

public:
	PCMStream(int rate, int channels, bool disposeStream, byte *data, uint32 size)
		: _rate(rate), _channels(channels), _data(data), _curSample(data), _size(size), _disposeAfterUse(disposeStream) {}

	virtual ~PCMStream() {
		if (_disposeAfterUse)
			delete[] _data;
	}

	int readBuffer(int16 *buffer, const int numSamples);
	int getChannels() const { return _channels; }
	bool endOfData() const { return ((size_t)_curSample - (size_t)_data) >= _size; }
	int getRate() const { return _rate; }
};

template<bool is16Bit, bool isUnsigned, bool isLE>
int PCMStream<is16Bit, isUnsigned, isLE>::readBuffer(int16 *buffer, const int numSamples) {
	int samples = numSamples;

	while (samples > 0 && !endOfData()) {
		*buffer++ = READ_ENDIAN_SAMPLE(is16Bit, isUnsigned, isLE);
		_curSample += is16Bit ? 2 : 1;
		samples--;
	}

	return numSamples - samples;
}

/* In the following, we use preprocessor / macro tricks to simplify the code
 * which instantiates the input streams. We used to use template functions for
 * this, but MSVC6 / EVC 3-4 (used for WinCE builds) are extremely buggy when it
 * comes to this feature of C++... so as a compromise we use macros to cut down
 * on the (source) code duplication a bit.
 * So while normally macro tricks are said to make maintenance harder, in this
 * particular case it should actually help it :-)
 */

#define MAKE_RAW_STREAM(UNSIGNED) \
	if (is16Bit) { \
		if (isLE) \
			return new PCMStream<true, UNSIGNED, true>(rate, channels, disposeAfterUse, data, size); \
		else  \
			return new PCMStream<true, UNSIGNED, false>(rate, channels, disposeAfterUse, data, size); \
	} else \
		return new PCMStream<false, UNSIGNED, false>(rate, channels, disposeAfterUse, data, size)


AudioStream *makePCMStream(byte *data, uint32 size, int rate, int channels, byte flags, bool disposeAfterUse) {
	const bool is16Bit    = (flags & FLAG_16BITS) != 0;
	const bool isUnsigned = (flags & FLAG_UNSIGNED) != 0;
	const bool isLE       = (flags & FLAG_LITTLE_ENDIAN) != 0;

	if (isUnsigned) {
		MAKE_RAW_STREAM(true);
	} else {
		MAKE_RAW_STREAM(false);
	}
}
