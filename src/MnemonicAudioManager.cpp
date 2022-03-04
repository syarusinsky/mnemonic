#include "MnemonicAudioManager.hpp"

#include "IMnemonicUiEventListener.hpp"
#include "AudioConstants.hpp"
#include <string.h>
#include "B12Compression.hpp"

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSram, unsigned int axiSramSizeInBytes) :
	m_AxiSramAllocator( axiSram, axiSramSizeInBytes ),
	m_FileManager( sdCard, &m_AxiSramAllocator ),
	m_TransportProgress( 0 ),
	m_AudioTracks(),
	m_DecompressedBuffer( m_AxiSramAllocator.allocatePrimativeArray<uint16_t>(ABUFFER_SIZE) ),
	m_MasterClockCount( 0 ),
	m_CurrentMaxLoopCount( MNEMONIC_NEOTRELLIS_COLS ), // 8 to avoid arithmetic exception when performing modulo
	m_MidiTracks(),
	m_MidiEventsToSend(),
	m_RecordingMidiState( MidiRecordingState::NOT_RECORDING ),
	m_TempMidiTrackEvents( reinterpret_cast<MidiTrackEvent*>(
				m_AxiSramAllocator.allocatePrimativeArray<uint8_t>(MNEMONIC_MAX_MIDI_TRACK_EVENTS * sizeof(MidiTrackEvent))) ),
	m_TempMidiTrackEventsIndex( 0 ),
	m_TempMidiTrackEventsNumEvents( 1 ), // 1 to avoid arithmetic exception when performing modulo
	m_TempMidiTrackEventsLoopEnd( 1 )
{
}

MnemonicAudioManager::~MnemonicAudioManager()
{
}

void MnemonicAudioManager::publishUiEvents()
{
	static unsigned int previousTransportProgress = 0;
	if ( m_TransportProgress != previousTransportProgress )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::TRANSPORT_MOVE, nullptr, 0, m_TransportProgress) );
		previousTransportProgress = m_TransportProgress;
	}
}

void MnemonicAudioManager::verifyFileSystem()
{
	if ( ! m_FileManager.isValidFatFileSystem() )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::INVALID_FILESYSTEM, nullptr, 0, 0) );
		this->unbindFromMnemonicParameterEventSystem();
	}
}

void MnemonicAudioManager::call (int16_t* writeBufferL, int16_t* writeBufferR)
{
	// update transport state
	const unsigned int numSamplesPerCell = m_CurrentMaxLoopCount / MNEMONIC_NEOTRELLIS_COLS;
	const unsigned int progressInCell = m_MasterClockCount % ( numSamplesPerCell );
	if ( progressInCell == 0 && m_CurrentMaxLoopCount > MNEMONIC_NEOTRELLIS_COLS )
	{
		m_TransportProgress = ( m_MasterClockCount / numSamplesPerCell ) % MNEMONIC_NEOTRELLIS_COLS;
	}

	// update midi recording state
	if ( m_RecordingMidiState == MidiRecordingState::WAITING_TO_RECORD && m_MasterClockCount == 0 )
	{
		m_RecordingMidiState = MidiRecordingState::RECORDING;
		m_TempMidiTrackEventsNumEvents = 1; // 1 to avoid arithmetic exception when performing modulo
		m_TempMidiTrackEventsIndex = 0;
		m_TempMidiTrackEventsLoopEnd = 1;
	}
	else if ( m_RecordingMidiState == MidiRecordingState::RECORDING && m_MasterClockCount == 0 )
	{
		this->endRecordingMidiTrack();
	}

	// update master clock count state
	m_MasterClockCount = ( m_MasterClockCount + 1 ) % m_CurrentMaxLoopCount;

	// fill buffer with audio track data
	for ( AudioTrack& audioTrack : m_AudioTracks )
	{
		audioTrack.call( writeBufferL, writeBufferR );

		if ( audioTrack.shouldLoop(m_MasterClockCount) )
		{
			audioTrack.play();
		}
	}

	// fill midi event queue with midi events at this time code
	for ( MidiTrack& midiTrack : m_MidiTracks )
	{
		midiTrack.addMidiEventsAtTimeCode( m_MasterClockCount, m_MidiEventsToSend );
	}

	// TODO add limiter stage
}

void MnemonicAudioManager::onMidiEvent (const MidiEvent& midiEvent)
{
	// TODO probably need to keep track of what midi messages have a note on without a note off, to apply those at the end of the loop
	if ( m_RecordingMidiState == MidiRecordingState::RECORDING && m_TempMidiTrackEventsIndex < MNEMONIC_MAX_MIDI_TRACK_EVENTS )
	{
		m_TempMidiTrackEvents[m_TempMidiTrackEventsIndex].m_MidiEvent = midiEvent;
		m_TempMidiTrackEvents[m_TempMidiTrackEventsIndex].m_TimeCode = m_MasterClockCount;

		m_TempMidiTrackEventsIndex++;

		m_MidiEventsToSend.push_back( midiEvent );
	}
	else
	{
		m_MidiEventsToSend.push_back( midiEvent );
	}
}

void MnemonicAudioManager::startRecordingMidiTrack()
{
	m_RecordingMidiState = MidiRecordingState::WAITING_TO_RECORD;
	// TODO we will need a separate function to delete midi tracks when we have cells to start and stop midi recordings
	if ( ! m_MidiTracks.empty() )
	{
		m_MidiTracks.pop_back();
	}
}

void MnemonicAudioManager::endRecordingMidiTrack()
{
	if ( m_RecordingMidiState == MidiRecordingState::RECORDING )
	{
		// TODO may also want to add note off messages to any messages that are still being 'pressed'? Probably do

		// end and finalize the temp midi track
		m_RecordingMidiState = MidiRecordingState::NOT_RECORDING;
		m_TempMidiTrackEventsNumEvents = ( m_TempMidiTrackEventsIndex != 0 ) ? m_TempMidiTrackEventsIndex : 1;
		m_TempMidiTrackEventsIndex = 0;
		if ( m_MasterClockCount == 0 )
		{
			m_TempMidiTrackEventsLoopEnd = m_CurrentMaxLoopCount;
		}
		else
		{
			const unsigned int numLoopsFit = m_CurrentMaxLoopCount / m_MasterClockCount;
			const unsigned int numLoopsFitRoundToEvenNum = ( numLoopsFit / 2 ) * 2;
			m_TempMidiTrackEventsLoopEnd = m_CurrentMaxLoopCount / numLoopsFitRoundToEvenNum;
		}

		// create the actual midi track and add it to the midi tracks vector
		if ( m_TempMidiTrackEventsNumEvents > 1 )
		{

			MidiTrack midiTrack( m_TempMidiTrackEvents, m_TempMidiTrackEventsNumEvents, m_TempMidiTrackEventsLoopEnd,
						m_AxiSramAllocator );
			m_MidiTracks.push_back( midiTrack );
		}
	}
}

void MnemonicAudioManager::onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent)
{
	PARAM_CHANNEL channel = static_cast<PARAM_CHANNEL>( paramEvent.getChannel() );
	unsigned int val = paramEvent.getValue();

	switch ( channel )
	{
		case PARAM_CHANNEL::ENTER_FILE_EXPLORER:
			this->enterFileExplorer();

			break;
		case PARAM_CHANNEL::LOAD_FILE:
			this->loadFile( val );

			break;
		case PARAM_CHANNEL::START_MIDI_RECORDING:
			this->startRecordingMidiTrack();

			break;
		case PARAM_CHANNEL::END_MIDI_RECORDING:
			this->endRecordingMidiTrack();

			break;
		default:
			break;
	}
}

void MnemonicAudioManager::enterFileExplorer()
{
	static uint8_t* primArrayPtr = nullptr;
	static unsigned int numEntries = 0;

	// since the sd card is not hot-pluggable, we only need to get the list of entries once
	if ( primArrayPtr == nullptr )
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
		primArrayPtr = m_AxiSramAllocator.allocatePrimativeArray<uint8_t>( sizeof(UiFileExplorerEntry) * uiEntryVec.size() );

		UiFileExplorerEntry* entryArrayPtr = reinterpret_cast<UiFileExplorerEntry*>( primArrayPtr );
		index = 0;
		for ( const UiFileExplorerEntry& entry : uiEntryVec )
		{
			entryArrayPtr[index] = entry;
			index++;
		}

		numEntries = uiEntryVec.size();
	}

	// send the ui event with all this data
	IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::ENTER_FILE_EXPLORER, primArrayPtr, numEntries, 0) );
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
				if ( trackInVec.isLoopable() )
				{
					trackInVec.setLoopable( false );
				}
				else
				{
					trackInVec.setLoopable( true );
				}

				if ( ! entryOtherChannel ) return;
			}
		}

		for ( AudioTrack& trackInVec : m_AudioTracks )
		{
			if ( trackR == trackInVec )
			{
				// the file is already loaded
				if ( trackInVec.isLoopable() )
				{
					trackInVec.setLoopable( false );
				}
				else
				{
					trackInVec.setLoopable( true );
				}

				return;
			}
		}

		m_AudioTracks.push_back( trackL );
		AudioTrack& trackLRef = m_AudioTracks[m_AudioTracks.size() - 1];
		// TODO right now we're assuming they're loopable, but in the future we need to make this a parameter
		trackLRef.setLoopable( true );
		trackLRef.setLoopLength( m_CurrentMaxLoopCount );
		if ( entryOtherChannel )
		{
			trackLRef.setAmplitudes( 1.0f, 0.0f );
			m_AudioTracks.push_back( trackR );
			AudioTrack& trackRRef = m_AudioTracks[m_AudioTracks.size() - 1];
			// TODO right now we're assuming they're loopable, but in the future we need to make this a parameter
			trackRRef.setLoopable( true );
			trackRRef.setLoopLength( m_CurrentMaxLoopCount );
			trackRRef.setAmplitudes( 0.0f, 1.0f );
		}

		// reset the loop length for all audio tracks
		for ( AudioTrack& trackInVec : m_AudioTracks )
		{
			trackInVec.setLoopLength( m_CurrentMaxLoopCount );
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
	else
	{
		return nullptr;
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
