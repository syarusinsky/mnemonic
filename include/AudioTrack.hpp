#ifndef AUDIOTRACK_HPP
#define AUDIOTRACK_HPP

/*************************************************************************
 * An AudioTrack defines a stream of double buffered b12 encoded audio
 * and utilities for decoding the data. The user should continuously
 * check the shouldFillNextBuffer function and fill the buffer accordingly.
*************************************************************************/

#include "AudioConstants.hpp"
#include "IBufferCallback.hpp"
#include "Fat16Entry.hpp"
#include <stdint.h>

class IAllocator;
class Fat16FileManager;

class AudioTrack : public IBufferCallback<int16_t, true>
{
	public:
		AudioTrack (Fat16FileManager& fileManager, const Fat16Entry& entry, unsigned int b12BufferSizes, IAllocator& allocator,
				uint16_t* decompressedBuffer);
		~AudioTrack() override;

		bool operator== (const AudioTrack& other) const;

		Fat16Entry getFatEntry() const { return m_FatEntry; }

		bool shouldFillNextBuffer();
		void fillNextBuffer (const uint8_t* const compressedBuf);

		void play();
		void reset();

		void setAmplitudes (const float amplitudeL, const float amplitudeR);

		void call (int16_t* writeBufferL, int16_t* writeBufferR) override;

		void freeData(); // frees the data in AXISRAM, should be called when unloading the track

	private:
		Fat16FileManager& 	m_FileManager;
		Fat16Entry 		m_FatEntry;

		IAllocator& 		m_Allocator;

		unsigned int 		m_B12BufferSize;

		unsigned int 		m_B12CircularBufferSize;
		uint8_t*     		m_B12CircularBuffer;
		unsigned int 		m_B12WritePos;
		unsigned int 		m_B12ReadPos;

		uint16_t*     		m_DecompressedBuffer;

		float 			m_AmplitudeL;
		float 			m_AmplitudeR;

		uint8_t* getBuffer (bool writeBuffer);

		bool shouldDecompress();
		void decompressToBuffer (int16_t* writeBufferL, int16_t* writeBufferR);
};

#endif // AUDIOTRACK_HPP
