#include "MnemonicAudioManager.hpp"

#include "IMnemonicUiEventListener.hpp"
#include "AudioConstants.hpp"
#include <string.h>
#include "B12Compression.hpp"

#include <iostream>

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSram, unsigned int axiSramSizeInBytes) :
	m_AxiSramAllocator( axiSram, axiSramSizeInBytes ),
	m_FileManager( sdCard, &m_AxiSramAllocator ),
	m_AudioTracks(),
	m_DecompressedBuffer( m_AxiSramAllocator.allocatePrimativeArray<uint16_t>(ABUFFER_SIZE) )
{
}

MnemonicAudioManager::~MnemonicAudioManager()
{
}

void MnemonicAudioManager::verifyFileSystem()
{
	if ( ! m_FileManager.isValidFatFileSystem() )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::INVALID_FILESYSTEM, nullptr, 0, 0) );
		this->unbindFromMnemonicParameterEventSystem();
	}
	else
	{
		this->enterFileExplorer();
	}
}

void MnemonicAudioManager::call (int16_t* writeBufferL, int16_t* writeBufferR)
{
	memset( writeBufferL, 0, ABUFFER_SIZE * sizeof(int16_t) );
	memset( writeBufferR, 0, ABUFFER_SIZE * sizeof(int16_t) );

	for ( AudioTrack& audioTrack : m_AudioTracks )
	{
		audioTrack.call( writeBufferL, writeBufferR );
	}

	// TODO add limiter stage
}

void MnemonicAudioManager::onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent)
{
	PARAM_CHANNEL channel = static_cast<PARAM_CHANNEL>( paramEvent.getChannel() );
	unsigned int val = paramEvent.getValue();

	switch ( channel )
	{
		case PARAM_CHANNEL::LOAD_FILE:
			this->loadFile( val );

			break;
		default:
			break;
	}
}

void MnemonicAudioManager::enterFileExplorer()
{
	// get all b12 file entries and place them in vector
	std::vector<UiFileExplorerEntry> uiEntryVec;
	unsigned int index = 0;
	for ( const Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		if ( ! entry->isDeletedEntry() && (strncmp(entry->getExtensionRaw(), "b12", FAT16_EXTENSION_SIZE) == 0
			|| strncmp(entry->getExtensionRaw(), "B12", FAT16_EXTENSION_SIZE) == 0) )
		{
			UiFileExplorerEntry uiEntry;
			strcpy( uiEntry.m_FilenameDisplay, entry->getFilenameDisplay() );
			uiEntry.m_Index = index;

			uiEntryVec.push_back( uiEntry );
		}

		index++;
	}

	// allocate them in contiguous memory for ui use
	// TODO might want to keep this pointer to free it later after viewing list?
	uint8_t* primArrayPtr = m_AxiSramAllocator.allocatePrimativeArray<uint8_t>( sizeof(UiFileExplorerEntry) * uiEntryVec.size() );
	UiFileExplorerEntry* entryArrayPtr = reinterpret_cast<UiFileExplorerEntry*>( primArrayPtr );
	index = 0;
	for ( const UiFileExplorerEntry& entry : uiEntryVec )
	{
		entryArrayPtr[index] = entry;
		index++;
	}

	// send the ui event with all this data
	IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::ENTER_FILE_EXPLORER, primArrayPtr, uiEntryVec.size(), 0) );
}

void MnemonicAudioManager::loadFile (unsigned int index)
{
	const Fat16Entry* entry = m_FileManager.getCurrentDirectoryEntries()[index];

	// find the stereo channel if available
	const Fat16Entry* entryOtherChannel = this->lookForOtherChannel( entry->getFilenameDisplay() );

	if ( ! entry->isDeletedEntry() && (strncmp(entry->getExtensionRaw(), "b12", FAT16_EXTENSION_SIZE) == 0
		|| strncmp(entry->getExtensionRaw(), "B12", FAT16_EXTENSION_SIZE) == 0) )
	{
		AudioTrack trackL( m_FileManager, *entry, m_FileManager.getActiveBootSector()->getSectorSizeInBytes(), m_AxiSramAllocator,
					m_DecompressedBuffer );
		AudioTrack trackR( m_FileManager, (entryOtherChannel) ? *entryOtherChannel : *entry,
					m_FileManager.getActiveBootSector()->getSectorSizeInBytes(), m_AxiSramAllocator, m_DecompressedBuffer );

		for ( AudioTrack& trackInVec : m_AudioTracks )
		{
			if ( trackL == trackInVec )
			{
				// the file is already loaded
				trackInVec.play();
				trackL.freeData();

				if ( ! entryOtherChannel ) return;
			}
		}

		for ( AudioTrack& trackInVec : m_AudioTracks )
		{
			if ( trackR == trackInVec )
			{
				// the file is already loaded
				trackInVec.play();
				trackR.freeData();

				return;
			}
		}

		m_AudioTracks.push_back( trackL );
		m_AudioTracks[m_AudioTracks.size() - 1].play();
		if ( entryOtherChannel )
		{
			m_AudioTracks[m_AudioTracks.size() - 1].setAmplitudes( 1.0f, 0.0f );
			m_AudioTracks.push_back( trackR );
			m_AudioTracks[m_AudioTracks.size() - 1].play();
			m_AudioTracks[m_AudioTracks.size() - 1].setAmplitudes( 0.0f, 1.0f );
		}
	}
}

const Fat16Entry* MnemonicAudioManager::lookForOtherChannel (const char* filenameDisplay)
{
	unsigned int dotIndex = 0;
	for ( unsigned int characterNum = 0; characterNum < FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2; characterNum++ )
	{
		if ( filenameDisplay[characterNum] == '.' )
		{
			dotIndex = characterNum;

			break;
		}
	}

	char filenameToLookFor[FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2];
	if ( filenameDisplay[dotIndex - 1] == 'L' ) // if we have the left channel, look for the right channel
	{
		strcpy( filenameToLookFor, filenameDisplay );
		filenameToLookFor[dotIndex - 1] = 'R';
	}
	else if ( filenameDisplay[dotIndex - 1] == 'R' ) // if we have the right channel, look for the left channel
	{
		strcpy( filenameToLookFor, filenameDisplay );
		filenameToLookFor[dotIndex - 1] = 'L';
	}

	for ( const Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		if ( strcmp(entry->getFilenameDisplay(), filenameToLookFor) == 0 )
		{
			return entry;
		}
	}

	return nullptr;
}
