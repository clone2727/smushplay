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

#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include "types.h"

/**
 * Generic audio input stream. Subclasses of this are used to feed arbitrary
 * sampled audio data into smushplay's audio mixer.
 */
class AudioStream {
public:
	virtual ~AudioStream() {}

	/**
	 * Fill the given buffer with up to numSamples samples. Returns the actual
	 * number of samples read, or -1 if a critical error occurred (note: you
	 * *must* check if this value is less than what you requested, this can
	 * happen when the stream is fully used up).
	 *
	 * Data has to be in native endianess, 16 bit per sample, signed. For stereo
	 * stream, buffer will be filled with interleaved left and right channel
	 * samples, starting with a left sample. Furthermore, the samples in the
	 * left and right are summed up. So if you request 4 samples from a stereo
	 * stream, you will get a total of two left channel and two right channel
	 * samples.
	 */
	virtual int readBuffer(int16 *buffer, const int numSamples) = 0;

	/** The number of channels in the stream */
	virtual int getChannels() const = 0;

	/** Sample rate of the stream. */
	virtual int getRate() const = 0;

	/**
	 * End of data reached? If this returns true, it means that at this
	 * time there is no data available in the stream. However there may be
	 * more data in the future.
	 * This is used by e.g. a rate converter to decide whether to keep on
	 * converting data or stop.
	 */
	virtual bool endOfData() const = 0;

	/**
	 * End of stream reached? If this returns true, it means that all data
	 * in this stream is used up and no additional data will appear in it
	 * in the future.
	 * This is used by the mixer to decide whether a given stream shall be
	 * removed from the list of active streams (and thus be destroyed).
	 * By default this maps to endOfData()
	 */
	virtual bool endOfStream() const { return endOfData(); }
};

class QueuingAudioStream : public AudioStream {
public:
	/**
	 * Queue an audio stream for playback. This stream plays all queued
	 * streams, in the order they were queued. If disposeAfterUse is set to
	 * true, then the queued stream is deleted after all data
	 * contained in it has been played.
	 */
	virtual void queueAudioStream(AudioStream *audStream, bool disposeAfterUse = true) = 0;

	/**
	 * Mark this stream as finished. That is, signal that no further data
	 * will be queued to it. Only after this has been done can this
	 * stream ever 'end'.
	 */
	virtual void finish() = 0;

	/**
	 * Return the number of streams still queued for playback (including
	 * the currently playing stream).
	 */
	virtual uint32 getQueuedStreamCount() const = 0;
};

/**
 * Factory function for an QueuingAudioStream.
 */
QueuingAudioStream *makeQueuingAudioStream(int rate, int channels);

#endif
