#ifndef AUDIOTRACK_HPP
#define AUDIOTRACK_HPP

/*************************************************************************
 * An AudioTrack defines a stream of double buffered b12 encoded audio
 * and utilities for decoding the data. The user should continuously
 * check the shouldFillNextBuffer function and fill the buffer accordingly.
*************************************************************************/

#include "AudioConstants.hpp"
#include "IBufferCallback.hpp"
#include <stdint.h>

class AudioTrack : public IBufferCallback<uint16_t>
{
	public:
		AudioTrack (unsigned int b12BufferSizes);
		~AudioTrack() override;

		bool shouldFillNextBuffer();
		void fillNextBuffer (const uint8_t* const compressedBuf);

		void reset();

		void call (uint16_t* writeBuffer) override;

	private:
		unsigned int 	m_B12BufferSize;

		unsigned int 	m_B12CircularBufferSize;
		uint8_t* 	m_B12CircularBuffer;
		unsigned int 	m_B12WritePos;
		unsigned int 	m_B12ReadPos;

		uint16_t 	m_DecompressedBuffer[ABUFFER_SIZE];

		uint8_t* getBuffer (bool writeBuffer);
};

#endif // AUDIOTRACK_HPP
