#ifndef MNEMONICAUDIOMANAGER_HPP
#define MNEMONICAUDIOMANAGER_HPP

/*************************************************************************
 * The MnemonicAudioManager is responsible for managing audio and midi
 * streams for mnemonic. This includes handling the timing between each
 * of the 'tracks'.
*************************************************************************/

#include "MnemonicConstants.hpp"
#include "IBufferCallback.hpp"
#include "Fat16FileManager.hpp"

class IStorageMedia;

class MnemonicAudioManager : public IBufferCallback<uint16_t>
{
	public:
		MnemonicAudioManager (IStorageMedia& storageMedia);
		~MnemonicAudioManager() override;

		void call (uint16_t* writeBuffer) override;

	private:
		Fat16FileManager m_FileManager;
};

#endif // MNEMONICAUDIOMANAGER_HPP
