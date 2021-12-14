#include "MnemonicAudioManager.hpp"

#include "AudioConstants.hpp"

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& storageMedia) :
	m_FileManager( storageMedia )
{
}

MnemonicAudioManager::~MnemonicAudioManager()
{
}

void MnemonicAudioManager::call (uint16_t* writeBuffer)
{
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
	}
}
