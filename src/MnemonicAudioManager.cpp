#include "MnemonicAudioManager.hpp"

#include "IMnemonicUiEventListener.hpp"
#include "AudioConstants.hpp"
#include <string.h>
#include "B12Compression.hpp"

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSram, unsigned int axiSramSizeInBytes) :
	m_AxiSramAllocator( axiSram, axiSramSizeInBytes ),
	m_FileManager( sdCard, &m_AxiSramAllocator ),
	m_AudioTrack( m_FileManager.getActiveBootSector()->getSectorSizeInBytes(), m_AxiSramAllocator ),
	m_AudioTrackEntry( nullptr ),
	m_CompressedBufferSize( m_FileManager.getActiveBootSector()->getSectorSizeInBytes() ),
	m_CompressedCircularBufferSize( m_CompressedBufferSize * 3 ),
	m_CompressedCircularBuffer( m_AxiSramAllocator.allocatePrimativeArray<uint8_t>(m_CompressedCircularBufferSize) ),
	m_CompressedCircularBufferIncr( 0 ),
	m_RecordDataToWrite( SharedData<uint8_t>::MakeSharedData(m_CompressedBufferSize * 3) ),
	m_RecordDataToWriteIncr( 0 ),
	m_Recording( false ),
	m_PlayingBack( false )
{
}

MnemonicAudioManager::~MnemonicAudioManager()
{
	if ( m_AudioTrackEntry ) m_AxiSramAllocator.free<Fat16Entry>( m_AudioTrackEntry );
}

void MnemonicAudioManager::verifyFileSystem()
{
	if ( ! m_FileManager.isValidFatFileSystem() )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::INVALID_FILESYSTEM, 0) );
		this->unbindFromMnemonicParameterEventSystem();
	}
	else
	{
		this->deleteExistingFile();
	}
}

void MnemonicAudioManager::call (uint16_t* writeBuffer)
{
	if ( m_Recording && m_AudioTrackEntry && m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
	{
		const unsigned int compressedDataSize = ( ABUFFER_SIZE * 2.0f ) * 0.75f;

		B12Compress( writeBuffer, ABUFFER_SIZE, &m_CompressedCircularBuffer[m_CompressedCircularBufferIncr], compressedDataSize );

		for ( unsigned int byte = 0; byte < compressedDataSize; byte++ )
		{
			m_RecordDataToWrite[m_RecordDataToWriteIncr] = m_CompressedCircularBuffer[m_CompressedCircularBufferIncr + byte];

			m_RecordDataToWriteIncr++;
			if ( m_RecordDataToWriteIncr == m_CompressedBufferSize * 3 )
			{
				m_FileManager.writeToEntry( *m_AudioTrackEntry, m_RecordDataToWrite );

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

				return;
			}
		}

		m_AudioTrack.call( writeBuffer );
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

			break;
		case PARAM_CHANNEL::TEST_2:
			// stop
			if ( m_Recording && m_AudioTrackEntry->getFileTransferInProgressFlagRef() )
			{
				m_FileManager.finalizeEntry( *m_AudioTrackEntry );
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
			}

			break;
		default:
			break;
	}
}

void MnemonicAudioManager::deleteExistingFile()
{
	if ( m_AudioTrackEntry ) m_AxiSramAllocator.free<Fat16Entry>( m_AudioTrackEntry );

	const char* trackEntryName = "tmpAud.b12";

	// look for the audio track in the file system
	unsigned int oldFileIndex = 0;
	std::vector<Fat16Entry*>& entriesInRoot = m_FileManager.getCurrentDirectoryEntries();
	for ( const Fat16Entry* entry : entriesInRoot )
	{
		if ( strcmp(entry->getFilenameDisplay(), trackEntryName) == 0 && ! entry->isDeletedEntry() )
		{
			m_FileManager.deleteEntry( oldFileIndex );

			break;
		}

		oldFileIndex++;
	}
}

void MnemonicAudioManager::createFile()
{
	m_AudioTrackEntry = m_AxiSramAllocator.allocate<Fat16Entry>( "tmpAud", "b12" );

	m_FileManager.createEntry( *m_AudioTrackEntry );
}
