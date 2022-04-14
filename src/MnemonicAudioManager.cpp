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
	m_TempMidiTrackEventsLoopEnd( 1 ),
	m_TempMidiTrackCellX( 0 ),
	m_TempMidiTrackCellY( 0 )
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

	for ( AudioTrack& audioTrack : m_AudioTracks )
	{
		if ( audioTrack.justFinished() )
		{
			IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::AUDIO_TRACK_FINISHED, nullptr, 0, 0,
								audioTrack.getCellX(), audioTrack.getCellY()) );
		}
	}

	if ( m_RecordingMidiState == MidiRecordingState::JUST_FINISHED_RECORDING )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::MIDI_RECORDING_FINISHED, nullptr, 0, 0,
							m_TempMidiTrackCellX, m_TempMidiTrackCellY) );
		m_RecordingMidiState = MidiRecordingState::NOT_RECORDING;
	}

	for ( MidiTrack& midiTrack : m_MidiTracks )
	{
		if ( midiTrack.justFinished() )
		{
			IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::MIDI_TRACK_FINISHED, nullptr, 0, 0,
								midiTrack.getCellX(), midiTrack.getCellY()) );
		}
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

		// stop any midi tracks in this row
		for ( MidiTrack& midiTrack : m_MidiTracks )
		{
			if ( m_TempMidiTrackCellY == midiTrack.getCellY() )
			{
				midiTrack.stop( true );
			}
		}
	}
	else if ( m_RecordingMidiState == MidiRecordingState::RECORDING && m_MasterClockCount == 0 )
	{
		this->endRecordingMidiTrack( m_TempMidiTrackCellX, m_TempMidiTrackCellY );
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
		midiTrack.waitForLoopStartOrEnd( m_MasterClockCount );

		if ( midiTrack.isPlaying() )
		{
			midiTrack.addMidiEventsAtTimeCode( m_MasterClockCount, m_MidiEventsToSend );
		}
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

void MnemonicAudioManager::startRecordingMidiTrack (unsigned int cellX, unsigned int cellY)
{
	m_TempMidiTrackCellX = cellX;
	m_TempMidiTrackCellY = cellY;
	m_RecordingMidiState = MidiRecordingState::WAITING_TO_RECORD;
}

void MnemonicAudioManager::endRecordingMidiTrack (unsigned int cellX, unsigned int cellY)
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

			MidiTrack midiTrack( cellX, cellY, m_TempMidiTrackEvents, m_TempMidiTrackEventsNumEvents, m_TempMidiTrackEventsLoopEnd,
						m_AxiSramAllocator );
			m_MidiTracks.push_back( midiTrack );

			m_MidiTracks.back().play( true );

			m_RecordingMidiState = MidiRecordingState::JUST_FINISHED_RECORDING;
		}
	}
}

void MnemonicAudioManager::saveMidiRecording (unsigned int cellX, unsigned int cellY, const char* nameWithoutExt)
{
	for ( MidiTrack& midiTrack : m_MidiTracks )
	{
		if ( cellX == midiTrack.getCellX() && cellY == midiTrack.getCellY() )
		{
			std::string filename( nameWithoutExt );
			// remove whitespace
			filename.erase( remove_if(filename.begin(), filename.end(), isspace), filename.end() );

			Fat16Entry entry( filename, "smf" );

			// check for a duplicate and delete if necessary
			unsigned int entryNum = 0;
			std::vector<Fat16Entry*>& dirEntries = m_FileManager.getCurrentDirectoryEntries();
			for ( const Fat16Entry* const entryPtr : dirEntries )
			{
				const char* entryPtrFilename = entryPtr->getFilenameDisplay();
				const char* newEntryFilename = entry.getFilenameDisplay();
				if ( strcmp(entryPtrFilename, newEntryFilename) == 0 )
				{
					m_FileManager.deleteEntry( entryNum );

					break;
				}

				entryNum++;
			}

			if ( m_FileManager.createEntry(entry) )
			{
				// get data necessary to reconstruct midi track
				SharedData<MidiTrackEvent> midiData = midiTrack.getData();
				unsigned int lenMd = midiTrack.getLengthInMidiTrackEvents();
				unsigned int lenBk = midiTrack.getLoopEndInBlocks();

				// write 'header' data first
				unsigned int sectorSizeInBytes = m_FileManager.getActiveBootSector()->getSectorSizeInBytes();
				SharedData<uint8_t> data = SharedData<uint8_t>::MakeSharedData( sectorSizeInBytes );
				data[ 0] = cellX; data[ 1] = cellX >> 1; data[ 2] = cellX >> 2; data[ 3] = cellX >> 3;
				data[ 4] = cellY; data[ 5] = cellY >> 1; data[ 6] = cellY >> 2; data[ 7] = cellY >> 3;
				data[ 8] = lenMd; data[ 9] = lenMd >> 1; data[10] = lenMd >> 2; data[11] = lenMd >> 3;
				data[12] = lenBk; data[13] = lenBk >> 1; data[14] = lenBk >> 2; data[15] = lenBk >> 3;
				if ( ! m_FileManager.writeToEntry(entry, data) ) goto fail;

				/* TODO this will be necessary to convert the header data back when loading
				cellX = data[ 0] | data[ 1] << 1 | data[ 2] << 2 | data[ 3] << 3;
				cellY = data[ 4] | data[ 5] << 1 | data[ 6] << 2 | data[ 7] << 3;
				lenMd = data[ 8] | data[ 9] << 1 | data[10] << 2 | data[11] << 3;
				lenBk = data[12] | data[13] << 1 | data[14] << 2 | data[15] << 3;
				*/

				// write midi event data
				unsigned int bytesWrittenToBlock = 0;
				for ( unsigned int midiTrackEventNum = 0; midiTrackEventNum < midiData.getSize(); midiTrackEventNum++ )
				{
					uint8_t* bytes = reinterpret_cast<uint8_t*>( &midiData[midiTrackEventNum] );
					for ( unsigned int byteNum = 0; byteNum < sizeof(MidiTrackEvent); byteNum++ )
					{
						data[bytesWrittenToBlock] = bytes[byteNum];

						bytesWrittenToBlock++;
						if ( bytesWrittenToBlock == sectorSizeInBytes )
						{
							bytesWrittenToBlock = 0;

							if ( ! m_FileManager.writeToEntry(entry, data) ) goto fail;
						}
					}
				}

				// flush data if necessary, or finalize entry if not
				if ( bytesWrittenToBlock != 0 )
				{
					if ( ! m_FileManager.flushToEntry(entry, data) ) goto fail;
				}
				else
				{
					if ( ! m_FileManager.finalizeEntry(entry) ) goto fail;
				}

				// send success message
				IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::MIDI_TRACK_RECORDING_STATUS, nullptr, 0,
									static_cast<unsigned int>(true)) );

				return;
			}

			goto fail;
		}
	}

fail:
	// send failed message
	IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::MIDI_TRACK_RECORDING_STATUS, nullptr, 0,
						static_cast<unsigned int>(false)) );
}

void MnemonicAudioManager::onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent)
{
	PARAM_CHANNEL channel = static_cast<PARAM_CHANNEL>( paramEvent.getChannel() );
	unsigned int cellX = paramEvent.getCellX();
	unsigned int cellY = paramEvent.getCellY();
	unsigned int val = paramEvent.getValue();

	switch ( channel )
	{
		case PARAM_CHANNEL::ENTER_FILE_EXPLORER:
			this->enterFileExplorer();

			break;
		case PARAM_CHANNEL::LOAD_FILE:
			this->loadFile( cellX, cellY, val );

			break;
		case PARAM_CHANNEL::UNLOAD_FILE:
			this->unloadFile( cellX, cellY );

			break;
		case PARAM_CHANNEL::PLAY_OR_STOP_TRACK:
			this->playOrStopTrack( cellX, cellY, static_cast<bool>(val) );

			break;
		case PARAM_CHANNEL::START_MIDI_RECORDING:
			this->startRecordingMidiTrack( cellX, cellY );

			break;
		case PARAM_CHANNEL::END_MIDI_RECORDING:
			this->endRecordingMidiTrack( cellX, cellY);

			break;
		case PARAM_CHANNEL::SAVE_MIDI_RECORDING:
			this->saveMidiRecording( cellX, cellY, paramEvent.getString() );

			break;
		default:
			break;
	}
}

void MnemonicAudioManager::enterFileExplorer()
{
	// TODO this is for b12 files, but need to filter similarly for midi files when entering file explorer for midi
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

void MnemonicAudioManager::playOrStopTrack (unsigned int cellX, unsigned int cellY, bool play)
{
	const MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
	{
		// if there is a track playing on one of these lanes, the other must not be playing any tracks
		bool shouldStopOtherLane = false;
		unsigned int stopLane = 0;
		if ( row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
		{
			shouldStopOtherLane = true;
			const unsigned int audioLoops2Lane = static_cast<unsigned int>( MNEMONIC_ROW::AUDIO_LOOPS_2 );
			const unsigned int audioOneshotsLane = static_cast<unsigned int>( MNEMONIC_ROW::AUDIO_ONESHOTS );
			stopLane = ( row == MNEMONIC_ROW::AUDIO_LOOPS_2 ) ? audioOneshotsLane : audioLoops2Lane;
		}

		for ( AudioTrack& audioTrack : m_AudioTracks )
		{
			if ( audioTrack.getCellX() == cellX && audioTrack.getCellY() == cellY )
			{
				if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 )
				{
					audioTrack.setLoopable( play );
				}
				else if ( row == MNEMONIC_ROW::AUDIO_ONESHOTS )
				{
					( play ) ? audioTrack.play() : audioTrack.reset();
				}
			}
			else if ( audioTrack.getCellY() == cellY )
			{
				// we should only have one track per lane playing at one time
				if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 )
				{
					audioTrack.setLoopable( false );
					IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::AUDIO_TRACK_FINISHED, nullptr, 0, 0,
										audioTrack.getCellX(), audioTrack.getCellY()) );
				}
				else if ( row == MNEMONIC_ROW::AUDIO_ONESHOTS )
				{
					audioTrack.reset();
					IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::AUDIO_TRACK_FINISHED, nullptr, 0, 0,
										audioTrack.getCellX(), audioTrack.getCellY()) );
				}
			}
			else if ( shouldStopOtherLane && audioTrack.getCellY() == stopLane )
			{
				audioTrack.setLoopable( false );
				audioTrack.reset();
				IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::AUDIO_TRACK_FINISHED, nullptr, 0, 0,
									audioTrack.getCellX(), audioTrack.getCellY()) );
			}
		}
	}
	else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
			|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
	{
		for ( MidiTrack& midiTrack : m_MidiTracks )
		{
			if ( play && midiTrack.getCellX() == cellX && midiTrack.getCellY() == cellY )
			{
				midiTrack.play();
			}
			else if ( ! play && midiTrack.getCellX() == cellX && midiTrack.getCellY() == cellY )
			{
				midiTrack.stop();
			}
			else if ( midiTrack.getCellY() == cellY )
			{
				// only one midi track per channel should play at once
				midiTrack.stop();
			}
		}
	}
}

void MnemonicAudioManager::loadFile (unsigned int cellX, unsigned int cellY, unsigned int index)
{
	MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
	{
		const Fat16Entry* entry = m_FileManager.getCurrentDirectoryEntries()[index];

		// find the stereo channel if available
		const Fat16Entry* entryOtherChannel = this->lookForOtherChannel( entry->getFilenameDisplay() );

		if ( ! entry->isDeletedEntry() && (strncmp(entry->getExtensionRaw(), "b12", FAT16_EXTENSION_SIZE) == 0
			|| strncmp(entry->getExtensionRaw(), "B12", FAT16_EXTENSION_SIZE) == 0) )
		{
			AudioTrack trackL( cellX, cellY, &m_FileManager, *entry, m_FileManager.getActiveBootSector()->getSectorSizeInBytes(),
						m_AxiSramAllocator, m_DecompressedBuffer );
			AudioTrack trackR( cellX, cellY, &m_FileManager, (entryOtherChannel) ? *entryOtherChannel : *entry,
						m_FileManager.getActiveBootSector()->getSectorSizeInBytes(), m_AxiSramAllocator,
						m_DecompressedBuffer );

			m_AudioTracks.push_back( trackL );
			AudioTrack& trackLRef = m_AudioTracks[m_AudioTracks.size() - 1];
			const bool isOneshot = static_cast<MNEMONIC_ROW>( cellY ) == MNEMONIC_ROW::AUDIO_ONESHOTS;
			if ( ! isOneshot ) trackLRef.setLoopLength( m_CurrentMaxLoopCount );
			if ( entryOtherChannel )
			{
				trackLRef.setAmplitudes( 1.0f, 0.0f );
				m_AudioTracks.push_back( trackR );
				AudioTrack& trackRRef = m_AudioTracks[m_AudioTracks.size() - 1];
				if ( ! isOneshot ) trackRRef.setLoopLength( m_CurrentMaxLoopCount );
				trackRRef.setAmplitudes( 0.0f, 1.0f );
			}

			// reset the loop length for all audio tracks
			for ( AudioTrack& trackInVec : m_AudioTracks )
			{
				if ( ! isOneshot ) trackInVec.setLoopLength( m_CurrentMaxLoopCount );
			}
		}
	}
}

void MnemonicAudioManager::unloadFile (unsigned int cellX, unsigned int cellY)
{
	MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
	{
		Fat16Entry* entryOtherChannel = nullptr;

		for ( std::vector<AudioTrack>::iterator trackInVecIt = m_AudioTracks.begin(); trackInVecIt != m_AudioTracks.end(); trackInVecIt++ )
		{
			AudioTrack& trackInVec = *trackInVecIt;
			if ( cellX == trackInVec.getCellX() && cellY == trackInVec.getCellY() )
			{
				// look for stereo entry before erasing track
				entryOtherChannel = this->lookForOtherChannel( trackInVec.getFatEntry().getFilenameDisplay() );

				m_AudioTracks.erase( trackInVecIt );

				break;
			}
		}

		if ( ! entryOtherChannel ) return;

		for ( std::vector<AudioTrack>::iterator trackInVecIt = m_AudioTracks.begin(); trackInVecIt != m_AudioTracks.end(); trackInVecIt++ )
		{
			AudioTrack& trackInVec = *trackInVecIt;
			if ( cellX == trackInVec.getCellX() && cellY == trackInVec.getCellY() )
			{
				m_AudioTracks.erase( trackInVecIt );

				break;
			}
		}
	}
	else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
			|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
	{
		for ( std::vector<MidiTrack>::iterator trackInVecIt = m_MidiTracks.begin(); trackInVecIt != m_MidiTracks.end(); trackInVecIt++ )
		{
			MidiTrack& midiTrack = *trackInVecIt;

			if ( midiTrack.getCellX() == cellX && midiTrack.getCellY() == cellY )
			{
				m_MidiTracks.erase( trackInVecIt );
			}

			break;
		}
	}
}

Fat16Entry* MnemonicAudioManager::lookForOtherChannel (const char* filenameDisplay)
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

	for ( Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		if ( strcmp(entry->getFilenameDisplay(), filenameToLookFor) == 0 )
		{
			return entry;
		}
	}

	return nullptr;
}
