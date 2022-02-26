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
#include "SharedData.hpp"
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

		unsigned int getFileLengthInAudioBlocks() { return m_FileLengthInAudioBlocks; }

		bool shouldFillNextBuffer();
		void fillNextBuffer (const uint8_t* const compressedBuf);

		void play();
		void reset();

		void setLoopable (const bool isLoopable);
		bool isLoopable() const { return m_IsLoopable; }
		void setLoopLength (unsigned int& currentMaxLoopLength); // also modifies the max loop length if this track is longer
		bool shouldLoop (const unsigned int masterClockCount); // loops if clock count is divisible

		void setAmplitudes (const float amplitudeL, const float amplitudeR);

		void call (int16_t* writeBufferL, int16_t* writeBufferR) override;

	private:
		Fat16FileManager& 	m_FileManager;
		Fat16Entry 		m_FatEntry;

		const unsigned int 	m_FileLengthInAudioBlocks;
		unsigned int 		m_LoopLengthInAudioBlocks;

		unsigned int 		m_B12BufferSize;

		unsigned int 		m_B12CircularBufferSize;
		SharedData<uint8_t>     m_B12CircularBuffer;
		unsigned int 		m_B12WritePos;
		unsigned int 		m_B12ReadPos;

		uint16_t*     		m_DecompressedBuffer;

		float 			m_AmplitudeL;
		float 			m_AmplitudeR;

		bool 			m_IsLoopable;

		uint8_t* getBuffer (bool writeBuffer);

		bool shouldDecompress();
		void decompressToBuffer (int16_t* writeBufferL, int16_t* writeBufferR);
};

#endif // AUDIOTRACK_HPP
