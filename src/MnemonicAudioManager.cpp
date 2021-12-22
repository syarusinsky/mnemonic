#include "MnemonicAudioManager.hpp"

#include "AudioConstants.hpp"
#include <string.h>
#include "B12Compression.hpp"

// TODO remove after testing
#include <iostream>

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& storageMedia) :
	m_FileManager( storageMedia ),
	m_AudioTrack( m_FileManager.getActiveBootSector()->getSectorSizeInBytes() ),
	m_AudioTrackEntry( nullptr ),
	m_CompressedBufferSize( m_FileManager.getActiveBootSector()->getSectorSizeInBytes() ),
	m_CompressedCircularBufferSize( m_CompressedBufferSize * 3 ),
	m_CompressedCircularBuffer( new uint8_t[m_CompressedCircularBufferSize] ),
	m_CompressedCircularBufferIncr( 0 ),
	m_RecordDataToWrite( SharedData<uint8_t>::MakeSharedData(m_CompressedBufferSize * 3) ),
	m_RecordDataToWriteIncr( 0 ),
	m_Recording( false ),
	m_PlayingBack( false )
{
	if ( ! m_FileManager.isValidFatFileSystem() )
	{
		std::cout << "NOT VALID FAT FILE SYSTEM, SHUTTING DOWN" << std::endl;
		std::exit( 0 );
	}

	this->deleteExistingFile();
}

MnemonicAudioManager::~MnemonicAudioManager()
{
	if ( m_AudioTrackEntry ) delete m_AudioTrackEntry;
}

void MnemonicAudioManager::call (uint16_t* writeBuffer)
{
	if ( m_Recording && m_AudioTrackEntry && m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
	{
		// TODO remove this after testing
		/*
		for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
		{
			writeBuffer[sample] = 0x0101;
		}
		*/

		const unsigned int compressedDataSize = ( ABUFFER_SIZE * 2.0f ) * 0.75f;

		if ( ! B12Compress(writeBuffer, ABUFFER_SIZE, &m_CompressedCircularBuffer[m_CompressedCircularBufferIncr], compressedDataSize) )
		{
			std::cout << "ERROR WHEN COMPRESSING!" << std::endl;
		}

		for ( unsigned int byte = 0; byte < compressedDataSize; byte++ )
		{
			m_RecordDataToWrite[m_RecordDataToWriteIncr] = m_CompressedCircularBuffer[m_CompressedCircularBufferIncr + byte];

			m_RecordDataToWriteIncr++;
			if ( m_RecordDataToWriteIncr == m_CompressedBufferSize * 3 )
			{
				if ( ! m_FileManager.writeToEntry(*m_AudioTrackEntry, m_RecordDataToWrite) )
				{
					std::cout << "FAILURE WRITING!" << std::endl;
				}

				m_RecordDataToWriteIncr = 0;
			}
		}

		m_CompressedCircularBufferIncr = ( m_CompressedCircularBufferIncr + compressedDataSize ) % m_CompressedCircularBufferSize;
	}
	else if ( m_PlayingBack && m_AudioTrackEntry && m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
	{
		while ( m_AudioTrack.shouldFillNextBuffer() )
		{
			SharedData<uint8_t> data = m_FileManager.getSelectedFileNextSector( *m_AudioTrackEntry );
			if ( m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
			{
				m_AudioTrack.fillNextBuffer( &data[0] );
			}
			else
			{
				m_PlayingBack = false;
				std::cout << "ENDING PLAYBACK" << std::endl;

				return;
			}
		}

		m_AudioTrack.call( writeBuffer );
		// TODO remove this after testing
		/*
		for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
		{
			if ( writeBuffer[sample] != 0x0101 )
			{
				writeBuffer[sample] = 0x0101;
			}
		}
		*/
	}
}

void MnemonicAudioManager::onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent)
{
	PARAM_CHANNEL channel = static_cast<PARAM_CHANNEL>( paramEvent.getChannel() );
	float val = paramEvent.getValue();

	switch ( channel )
	{
		case PARAM_CHANNEL::TEST_1:
			// record
			this->deleteExistingFile();
			this->createFile();
			m_Recording = true;
			std::cout << "Created audio file" << std::endl;

			break;
		case PARAM_CHANNEL::TEST_2:
			// stop
			if ( m_Recording && m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
			{
				m_FileManager.finalizeEntry( *m_AudioTrackEntry );
				std::cout << "Finalized audio file" << std::endl;
			}
			m_Recording = false;
			m_PlayingBack = false;

			break;
		case PARAM_CHANNEL::TEST_3:
			// play
			if ( ! m_Recording && m_AudioTrackEntry )
			{
				m_AudioTrack.reset();
				m_FileManager.readEntry( *m_AudioTrackEntry );
				m_PlayingBack = true;

				std::cout << "Playback started" << std::endl;
			}

			break;
		default:
			break;
	}
}

void MnemonicAudioManager::deleteExistingFile()
{
	if ( m_AudioTrackEntry ) delete m_AudioTrackEntry;

	const char* trackEntryName = "tmpAud.b12";

	// look for the audio track in the file system
	unsigned int oldFileIndex = 0;
	std::vector<Fat16Entry>& entriesInRoot = m_FileManager.getCurrentDirectoryEntries();
	for ( const Fat16Entry& entry : entriesInRoot )
	{
		if ( strcmp(entry.getFilenameDisplay(), trackEntryName) == 0 )
		{
			m_FileManager.deleteEntry( oldFileIndex );
			std::cout << "Found existing file, deleting" << std::endl;

			break;
		}

		oldFileIndex++;
	}
}

void MnemonicAudioManager::createFile()
{
	m_AudioTrackEntry = new Fat16Entry( "tmpAud", "b12" );

	m_FileManager.createEntry( *m_AudioTrackEntry );
}
