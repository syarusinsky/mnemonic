#ifndef MNEMONICAUDIOMANAGER_HPP
#define MNEMONICAUDIOMANAGER_HPP

/*************************************************************************
 * The MnemonicAudioManager is responsible for managing audio and midi
 * streams for mnemonic. This includes handling the timing between each
 * of the 'tracks'.
*************************************************************************/

#include "AudioTrack.hpp"
#include "MnemonicConstants.hpp"
#include "IBufferCallback.hpp"
#include "Fat16FileManager.hpp"
#include "IMnemonicParameterEventListener.hpp"
#include "IAllocator.hpp"

class IStorageMedia;

class MnemonicAudioManager : public IBufferCallback<uint16_t>, public IMnemonicParameterEventListener
{
	public:
		MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSramPtr, unsigned int axiSramSizeInBytes);
		~MnemonicAudioManager() override;

		void verifyFileSystem(); // should be called on boot up at the very least

		void call (uint16_t* writeBuffer) override;

		void testFileCreationAndWrite();

		void onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent) override;

	private:
		IAllocator 		m_AxiSramAllocator;
		Fat16FileManager 	m_FileManager;
		AudioTrack 		m_AudioTrack;
		Fat16Entry* 		m_AudioTrackEntry;

		unsigned int 		m_CompressedBufferSize;
		unsigned int 		m_CompressedCircularBufferSize;
		uint8_t* 		m_CompressedCircularBuffer;
		unsigned int 		m_CompressedCircularBufferIncr;
		SharedData<uint8_t> 	m_RecordDataToWrite;
		unsigned int 		m_RecordDataToWriteIncr;

		bool 			m_Recording;
		bool 			m_PlayingBack;

		void deleteExistingFile();
		void createFile();
};

#endif // MNEMONICAUDIOMANAGER_HPP
