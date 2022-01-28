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

class MnemonicAudioManager : public IBufferCallback<int16_t>, public IMnemonicParameterEventListener
{
	public:
		MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSramPtr, unsigned int axiSramSizeInBytes);
		~MnemonicAudioManager() override;

		void verifyFileSystem(); // should be called on boot up at the very least

		void call (int16_t* writeBuffer) override;

		void onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent) override;

	private:
		IAllocator 			m_AxiSramAllocator;
		Fat16FileManager 		m_FileManager;

		std::vector<AudioTrack> 	m_AudioTracks;

		uint16_t* 			m_DecompressedBuffer; // for holding decompressed audio buffers for all audio tracks

		void loadFile (unsigned int index);

		void enterFileExplorer();
};

#endif // MNEMONICAUDIOMANAGER_HPP
