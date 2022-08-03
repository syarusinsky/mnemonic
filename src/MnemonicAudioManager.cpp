#include "MnemonicAudioManager.hpp"

#include "IMnemonicUiEventListener.hpp"
#include "AudioConstants.hpp"
#include <string.h>
#include "B12Compression.hpp"
#include <ctype.h>

MnemonicAudioManager::MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSram, unsigned int axiSramSizeInBytes) :
	m_AxiSramAllocator( axiSram, axiSramSizeInBytes ),
	m_FileManager( sdCard, &m_AxiSramAllocator ),
	m_CurrentDirectory( Directory::ROOT ),
	m_TransportProgress( 0 ),
	m_AudioTracks(),
	m_DecompressedBuffer( m_AxiSramAllocator.allocatePrimativeArray<uint16_t>(ABUFFER_SIZE) ),
	m_MasterClockCount( 0 ),
	m_CurrentMaxLoopCount( MNEMONIC_NEOTRELLIS_COLS ), // 8 to avoid arithmetic exception when performing modulo
	m_ActiveMidiChannel( 1 ),
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
			this->resetLoopingInfo();
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
			this->resetLoopingInfo();
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

	this->resetLoopingInfo();

	// TODO add limiter stage
}

void MnemonicAudioManager::onMidiEvent (const MidiEvent& midiEvent)
{
	// TODO need to make this part of the class
	unsigned int midiChannel = m_ActiveMidiChannel;
	MidiEvent midiEventWithChannel = midiEvent;
	midiEventWithChannel.setChannel( midiChannel );

	// TODO probably need to keep track of what midi messages have a note on without a note off, to apply those at the end of the loop
	if ( m_RecordingMidiState == MidiRecordingState::RECORDING && m_TempMidiTrackEventsIndex < MNEMONIC_MAX_MIDI_TRACK_EVENTS )
	{
		m_TempMidiTrackEvents[m_TempMidiTrackEventsIndex].m_MidiEvent = midiEventWithChannel;
		m_TempMidiTrackEvents[m_TempMidiTrackEventsIndex].m_TimeCode = m_MasterClockCount;

		m_TempMidiTrackEventsIndex++;

		m_MidiEventsToSend.push_back( midiEventWithChannel );
	}
	else
	{
		m_MidiEventsToSend.push_back( midiEventWithChannel );
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
			if ( ! this->goToDirectory(Directory::MIDI) ) return;

			std::string filename( nameWithoutExt );
			// remove whitespace
			filename.erase( remove_if(filename.begin(), filename.end(), isspace), filename.end() );

			Fat16Entry entry( filename, "SMF" );

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
				SharedData<uint8_t> data = SharedData<uint8_t>::MakeSharedData( sectorSizeInBytes, &m_AxiSramAllocator );
				data[0] = lenMd; data[1] = lenMd >> 1; data[2] = lenMd >> 2; data[3] = lenMd >> 3;
				data[4] = lenBk; data[5] = lenBk >> 1; data[6] = lenBk >> 2; data[7] = lenBk >> 3;
				if ( ! m_FileManager.writeToEntry(entry, data) ) goto fail;

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

				midiTrack.setIsSaved( entry.getFilenameDisplay() );

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

void MnemonicAudioManager::saveScene (const char* nameWithoutExt)
{
	std::string fileData = "";

	// add version data
	fileData += "VER: " + std::string(MNEMONIC_SCENE_VERSION) + '\n';

	// add all audio file names with neotrellis cell locations
	for ( const AudioTrack& audioTrack : m_AudioTracks )
	{
		fileData += "AUDIO: " + std::to_string(audioTrack.getCellX()) + "," + std::to_string(audioTrack.getCellY());
		fileData += " " + std::string(audioTrack.getFatEntry().getFilenameDisplay()) + '\n';
	}

	// add all midi file names with neotrellis cell locations
	for ( const MidiTrack& midiTrack : m_MidiTracks )
	{
		// if there are any unsaved midi tracks, report to the user that they need to be saved
		if ( ! midiTrack.isSaved() )
		{
			IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::MIDI_TRACK_NOT_SAVED, nullptr, 0,
								0, midiTrack.getCellX(), midiTrack.getCellY()) );
			return;
		}

		fileData += "MIDI: " + std::to_string(midiTrack.getCellX()) + "," + std::to_string(midiTrack.getCellY());
		fileData += " " + std::string(midiTrack.getFilenameDisplay()) + '\n';
	}

	const char* fileDataCStr = fileData.c_str();
	SharedData<uint8_t> data = SharedData<uint8_t>::MakeSharedData( fileData.length() );
	for ( unsigned int character = 0; character < fileData.length(); character++ )
	{
		data[character] = fileDataCStr[character];
	}

	if ( ! this->goToDirectory(Directory::SCENE) ) return;

	std::string filename( nameWithoutExt );
	// remove whitespace
	filename.erase( remove_if(filename.begin(), filename.end(), isspace), filename.end() );
	Fat16Entry entry( filename, "SCN" );

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
		if ( ! m_FileManager.flushToEntry(entry, data) ) goto fail;

		// send success message
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::SCENE_SAVING_STATUS, nullptr, 0,
							static_cast<unsigned int>(true)) );

		return;
	}

fail:
	// send failed message
	IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::SCENE_SAVING_STATUS, nullptr, 0,
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
		case PARAM_CHANNEL::LOAD_AUDIO_RECORDING:
			this->enterFileExplorer( Directory::AUDIO );

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
		case PARAM_CHANNEL::LOAD_MIDI_RECORDING:
			this->enterFileExplorer( Directory::MIDI );

			break;
		case PARAM_CHANNEL::SAVE_SCENE:
			this->saveScene( paramEvent.getString() );

			break;
		case PARAM_CHANNEL::LOAD_SCENE:
			this->enterFileExplorer( Directory::SCENE );

			break;
		case PARAM_CHANNEL::ACTIVE_MIDI_CHANNEL:
			m_ActiveMidiChannel = paramEvent.getValue();

			break;
		case PARAM_CHANNEL::DELETE_FILE:
		{
			const MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );
			if ( row == MNEMONIC_ROW::TRANSPORT )
			{
				this->enterFileExplorer( Directory::SCENE );
			}
			else if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
			{
				// Not allowing this currently since audio issues would occur if the audio file was currently being played,
				// this could be fixed in the future but since I'm going to only be adding/removing audio files with a computer
				// anyways it's not worth the effort
				// this->enterFileExplorer( Directory::AUDIO );
			}
			else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
					|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
			{
				this->enterFileExplorer( Directory::MIDI );
			}
		}

			break;
		case PARAM_CHANNEL::CONFIRM_DELETE_FILE:
			this->deleteFile( val );

			break;
		default:
			break;
	}
}

uint8_t* MnemonicAudioManager::enterFileExplorerHelper (const char* extension, unsigned int& numEntries, uint8_t* previousPtr)
{
	// look for extensions both upper and lower case
	char extensionLowerCase[FAT16_EXTENSION_SIZE + 1];
	char extensionUpperCase[FAT16_EXTENSION_SIZE + 1];
	strcpy( extensionLowerCase, extension );
	strcpy( extensionUpperCase, extension );
	for ( unsigned int charNum = 0; charNum < FAT16_EXTENSION_SIZE + 1; charNum++ )
	{
		extensionLowerCase[charNum] = tolower( extensionLowerCase[charNum] );
		extensionUpperCase[charNum] = toupper( extensionUpperCase[charNum] );
	}

	// get all b12 file entries and place them in vector
	std::vector<UiFileExplorerEntry> uiEntryVec;
	unsigned int index = 0;
	for ( const Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		if ( ! entry->isDeletedEntry() && (strncmp(entry->getExtensionRaw(), extensionLowerCase, FAT16_EXTENSION_SIZE) == 0
			|| strncmp(entry->getExtensionRaw(), extensionUpperCase, FAT16_EXTENSION_SIZE) == 0) )
		{
			UiFileExplorerEntry uiEntry;
			strcpy( uiEntry.m_FilenameDisplay, entry->getFilenameDisplay() );
			uiEntry.m_Index = index;

			uiEntryVec.push_back( uiEntry );
		}

		index++;
	}

	// free the previous data if necessary
	if ( previousPtr ) m_AxiSramAllocator.free( previousPtr );

	// allocate them in contiguous memory for ui use
	uint8_t* primArrayPtr = m_AxiSramAllocator.allocatePrimativeArray<uint8_t>( sizeof(UiFileExplorerEntry) * uiEntryVec.size() );

	UiFileExplorerEntry* entryArrayPtr = reinterpret_cast<UiFileExplorerEntry*>( primArrayPtr );
	index = 0;
	for ( const UiFileExplorerEntry& entry : uiEntryVec )
	{
		entryArrayPtr[index] = entry;
		index++;
	}

	numEntries = uiEntryVec.size();

	return primArrayPtr;
}

void MnemonicAudioManager::enterFileExplorer (const Directory& dir)
{
	static uint8_t* b12PrimArrayPtr = nullptr;
	static unsigned int b12NumEntries = 0;
	static uint8_t* midPrimArrayPtr = nullptr;
	static unsigned int midNumEntries = 0;
	static uint8_t* scnPrimArrayPtr = nullptr;
	static unsigned int scnNumEntries = 0;

	// since the sd card is not hot-pluggable, we only need to get the list of audio entries once
	if ( dir == Directory::AUDIO && b12PrimArrayPtr == nullptr )
	{
		this->goToDirectory( Directory::AUDIO );
		b12PrimArrayPtr = this->enterFileExplorerHelper( "B12", b12NumEntries, b12PrimArrayPtr );
	}
	else if ( dir == Directory::MIDI )
	{
		this->goToDirectory( Directory::MIDI );
		midPrimArrayPtr = this->enterFileExplorerHelper( "SMF", midNumEntries, midPrimArrayPtr );
	}
	else if ( dir == Directory::SCENE )
	{
		this->goToDirectory( Directory::SCENE );
		scnPrimArrayPtr = this->enterFileExplorerHelper( "SCN", scnNumEntries, scnPrimArrayPtr );
	}

	// send the ui event with all this data
	if ( dir == Directory::AUDIO )
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::ENTER_FILE_EXPLORER, b12PrimArrayPtr, b12NumEntries, 0) );
	}
	else if ( dir == Directory::MIDI ) // filter midi files instead
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::ENTER_FILE_EXPLORER, midPrimArrayPtr, midNumEntries, 1) );
	}
	else if ( dir == Directory::SCENE ) // filter scene files instead
	{
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::ENTER_FILE_EXPLORER, scnPrimArrayPtr, scnNumEntries, 2) );
	}
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

		bool otherTrackIsPlayingOnThisLane = false;
		for ( AudioTrack& audioTrack : m_AudioTracks )
		{
			if ( audioTrack.getCellY() == cellY && audioTrack.getCellX() != cellX && audioTrack.isPlaying() )
			{
				otherTrackIsPlayingOnThisLane = true;
				break;
			}
		}

		for ( AudioTrack& audioTrack : m_AudioTracks )
		{
			if ( audioTrack.getCellX() == cellX && audioTrack.getCellY() == cellY )
			{
				if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 )
				{
					audioTrack.setLoopable( play, otherTrackIsPlayingOnThisLane );
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
					audioTrack.setLoopable( false, otherTrackIsPlayingOnThisLane );
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
		bool otherTrackIsPlayingOnThisLane = false;
		for ( MidiTrack& midiTrack : m_MidiTracks )
		{
			if ( midiTrack.getCellY() == cellY && midiTrack.getCellX() != cellX && midiTrack.isPlaying() )
			{
				otherTrackIsPlayingOnThisLane = true;
				break;
			}
		}

		for ( MidiTrack& midiTrack : m_MidiTracks )
		{
			if ( play && midiTrack.getCellX() == cellX && midiTrack.getCellY() == cellY )
			{
				midiTrack.play( false, otherTrackIsPlayingOnThisLane );
			}
			else if ( ! play && midiTrack.getCellX() == cellX && midiTrack.getCellY() == cellY )
			{
				midiTrack.stop( false, otherTrackIsPlayingOnThisLane );
			}
			else if ( midiTrack.getCellY() == cellY )
			{
				// only one midi track per channel should play at once
				midiTrack.stop( false, otherTrackIsPlayingOnThisLane );
			}
		}
	}
}

void MnemonicAudioManager::loadFile (unsigned int cellX, unsigned int cellY, unsigned int index)
{
	MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( row == MNEMONIC_ROW::TRANSPORT )
	{
		if ( ! this->goToDirectory(Directory::SCENE) ) return;

		Fat16Entry* entry = m_FileManager.getCurrentDirectoryEntries()[index];

		// load the scene file contents into a string
		std::string sceneStr;
		m_FileManager.readEntry( *entry );
		unsigned int bytesRead = 0;
		while ( entry->getFileTransferInProgressFlagRef() )
		{
			SharedData<uint8_t> data = m_FileManager.getSelectedFileNextSector( *entry );
			for ( unsigned int byte = 0; byte < data.getSize() && bytesRead < entry->getFileSizeInBytes(); byte++ )
			{
				sceneStr += static_cast<char>( data[byte] );
				bytesRead++;
			}
		}

		// parse and get version
		if ( sceneStr.find("VER:") != std::string::npos )
		{
			size_t spacePos = sceneStr.find( " " );
			std::string versionStr = sceneStr.substr( spacePos + 1, sceneStr.find('\n') - spacePos - 1 );
			if ( ! this->loadSceneFileHelper(versionStr, sceneStr) )
			{
				// TODO ui should display error
			}
		}
		else
		{
			// TODO ui should display error
		}
	}
	else if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
	{
		if ( ! this->loadAudioFileHelper(index, cellX, cellY) )
		{
			// TODO ui should display error
		}
	}
	else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
			|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
	{
		if ( ! this->loadMidiFileHelper(index, cellX, cellY) )
		{
			// TODO ui should display error
		}
	}
}

void MnemonicAudioManager::unloadFile (unsigned int cellX, unsigned int cellY)
{
	MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS )
	{
		if ( ! this->goToDirectory(Directory::AUDIO) ) return;

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

		if ( entryOtherChannel )
		{
			for ( std::vector<AudioTrack>::iterator trackInVecIt = m_AudioTracks.begin(); trackInVecIt != m_AudioTracks.end();
					trackInVecIt++ )
			{
				AudioTrack& trackInVec = *trackInVecIt;
				if ( cellX == trackInVec.getCellX() && cellY == trackInVec.getCellY() )
				{
					m_AudioTracks.erase( trackInVecIt );

					break;
				}
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

				break;
			}
		}
	}
}

void MnemonicAudioManager::deleteFile (unsigned int index)
{
	m_FileManager.deleteEntry( index );
}

void MnemonicAudioManager::resetLoopingInfo()
{
	// reset looping info if necessary
	unsigned int maxLoopCount = MNEMONIC_NEOTRELLIS_COLS; // 8 to avoid arithmetic exception when performing modulo
	for ( MidiTrack& midiTrack : m_MidiTracks )
	{
		if ( midiTrack.isPlaying() )
		{
			const unsigned int loopLenInBlocks = midiTrack.getLoopEndInBlocks();
			maxLoopCount = ( maxLoopCount < loopLenInBlocks ) ? loopLenInBlocks : maxLoopCount;
		}
	}

	for ( AudioTrack& audioTrack : m_AudioTracks )
	{
		if ( audioTrack.isPlaying() )
		{
			const unsigned int loopLenInBlocks = audioTrack.getFileLengthInAudioBlocks();
			maxLoopCount = ( maxLoopCount < loopLenInBlocks ) ? loopLenInBlocks : maxLoopCount;
		}
	}

	if ( maxLoopCount < m_CurrentMaxLoopCount )
	{
		m_CurrentMaxLoopCount = maxLoopCount;
	}
	else if ( maxLoopCount > m_CurrentMaxLoopCount )
	{
		m_CurrentMaxLoopCount = maxLoopCount;
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

bool MnemonicAudioManager::goToDirectory (const Directory& directory)
{
	if ( m_CurrentDirectory == Directory::AUDIO || m_CurrentDirectory == Directory::MIDI || m_CurrentDirectory == Directory::SCENE )
	{
		// go back to root directory
		m_FileManager.selectEntry( 1 );
		m_CurrentDirectory = Directory::ROOT;
	}

	char dirName[FAT16_FILENAME_SIZE + 1] = { '\0' };
	unsigned int numChars = 0;
	if ( directory == Directory::AUDIO )
	{
		strcpy( dirName, "AUDIO" );
		numChars = 5;
	}
	else if ( directory == Directory::MIDI )
	{
		strcpy( dirName, "MIDI" );
		numChars = 4;
	}
	else if ( directory == Directory::SCENE )
	{
		strcpy( dirName, "SCENE" );
		numChars = 5;
	}

	if ( directory != Directory::ROOT )
	{
		unsigned int entryNum = 0;
		for ( Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
		{
			if ( strncmp(entry->getFilenameRaw(), dirName, numChars) == 0 )
			{
				m_FileManager.selectEntry( entryNum );
				m_CurrentDirectory = directory;
				return true;
			}

			entryNum++;
		}

		// we haven't found the directory, so send invalid filesystem message and return false
		IMnemonicUiEventListener::PublishEvent( MnemonicUiEvent(UiEventType::INVALID_FILESYSTEM, nullptr, 0, 0) );
		this->unbindFromMnemonicParameterEventSystem();
		return false;
	}

	return true;
}

bool MnemonicAudioManager::loadSceneFileHelper (const std::string& versionStr, std::string& sceneStr)
{
	if ( versionStr == "1.0.0" )
	{
		// first erase the version line
		sceneStr.erase( 0, sceneStr.find('\n') + 1 );

		// load each audio file
		while ( sceneStr.find("AUDIO: ") != std::string::npos )
		{
			sceneStr.erase( 0, sceneStr.find(" ") + 1 );
			unsigned int cellX = std::stoi( sceneStr.substr(0, sceneStr.find(",")) );
			sceneStr.erase( 0, sceneStr.find(",") + 1 );
			unsigned int cellY = std::stoi( sceneStr.substr(0, sceneStr.find(" ")) );
			sceneStr.erase( 0, sceneStr.find(" ") + 1 );
			std::string audioFilename = sceneStr.substr( 0, sceneStr.find('\n') );
			sceneStr.erase( 0, sceneStr.find('\n') + 1 );
			if ( ! this->loadAudioFileHelper(audioFilename, cellX, cellY) ) return false;
		}

		// load each midi file
		while ( sceneStr.find("MIDI: ") != std::string::npos )
		{
			sceneStr.erase( 0, sceneStr.find(" ") + 1 );
			unsigned int cellX = std::stoi( sceneStr.substr(0, sceneStr.find(",")) );
			sceneStr.erase( 0, sceneStr.find(",") + 1 );
			unsigned int cellY = std::stoi( sceneStr.substr(0, sceneStr.find(" ")) );
			sceneStr.erase( 0, sceneStr.find(" ") + 1 );
			std::string midiFilename = sceneStr.substr( 0, sceneStr.find('\n') );
			sceneStr.erase( 0, sceneStr.find('\n') + 1 );
			if ( ! this->loadMidiFileHelper(midiFilename, cellX, cellY) ) return false;
		}

		return true;
	}

	return false;
}

bool MnemonicAudioManager::loadAudioFileHelper (const std::string& filename, unsigned int cellX, unsigned int cellY)
{
	if ( ! this->goToDirectory(Directory::AUDIO) ) return false;

	unsigned int index = 0;
	for ( const Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		std::string filenameUppercaseExt = filename;
		std::string filenameLowercaseExt = filename;
		filenameUppercaseExt.replace( filenameUppercaseExt.find(".") + 1, 3, "B12" );
		filenameLowercaseExt.replace( filenameLowercaseExt.find(".") + 1, 3, "b12" );
		if ( strcmp(entry->getFilenameDisplay(), filenameUppercaseExt.c_str()) == 0
			|| strcmp(entry->getFilenameDisplay(), filenameLowercaseExt.c_str()) == 0 )
		{
			return this->loadAudioFileHelper( index, cellX, cellY, false );
		}

		index++;
	}

	return false;
}

bool MnemonicAudioManager::loadAudioFileHelper (unsigned int index, unsigned int cellX, unsigned int cellY, bool loadStereo)
{
	if ( ! this->goToDirectory(Directory::AUDIO) ) return false;

	const Fat16Entry* entry = m_FileManager.getCurrentDirectoryEntries()[index];

	// find the stereo channel if available
	const Fat16Entry* entryOtherChannel = ( loadStereo ) ? this->lookForOtherChannel( entry->getFilenameDisplay() ) : nullptr;

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

		IMnemonicUiEventListener::PublishEvent(
				MnemonicUiEvent(UiEventType::SCENE_TRACK_FILE_LOADED, nullptr, 0, 0, cellX, cellY) );

		return true;
	}

	return false;
}

bool MnemonicAudioManager::loadMidiFileHelper (const std::string& filename, unsigned int cellX, unsigned int cellY)
{
	if ( ! this->goToDirectory(Directory::MIDI) ) return false;

	unsigned int index = 0;
	for ( const Fat16Entry* entry : m_FileManager.getCurrentDirectoryEntries() )
	{
		std::string filenameUppercaseExt = filename;
		std::string filenameLowercaseExt = filename;
		filenameUppercaseExt.replace( filenameUppercaseExt.find(".") + 1, 3, "smf" );
		filenameLowercaseExt.replace( filenameLowercaseExt.find(".") + 1, 3, "SMF" );
		if ( strcmp(entry->getFilenameDisplay(), filenameUppercaseExt.c_str()) == 0
			|| strcmp(entry->getFilenameDisplay(), filenameLowercaseExt.c_str()) == 0 )
		{
			return this->loadMidiFileHelper( index, cellX, cellY );
		}

		index++;
	}

	return false;
}

bool MnemonicAudioManager::loadMidiFileHelper (unsigned int index, unsigned int cellX, unsigned int cellY)
{
	if ( ! this->goToDirectory(Directory::MIDI) ) return false;

	Fat16Entry entry = *m_FileManager.getCurrentDirectoryEntries()[index];

	if ( ! entry.isDeletedEntry() && (strncmp(entry.getExtensionRaw(), "smf", FAT16_EXTENSION_SIZE) == 0
		|| strncmp(entry.getExtensionRaw(), "SMF", FAT16_EXTENSION_SIZE) == 0) )
	{
		m_FileManager.readEntry( entry );

		// load header info
		SharedData<uint8_t> data = m_FileManager.getSelectedFileNextSector( entry );
		unsigned int lenMd = 0;
		unsigned int lenBk = 0;
		lenMd = data[0] | data[1] << 1 | data[2] << 2 | data[3] << 3;
		lenBk = data[4] | data[5] << 1 | data[6] << 2 | data[7] << 3;

		// load midi track events
		uint8_t* midiTrackEventPrimArr = m_AxiSramAllocator.allocatePrimativeArray<uint8_t>( lenMd * sizeof(MidiTrackEvent) );
		unsigned int midiTrackEventPrimArrIndex = 0;
		unsigned int sectorSizeInBytes = m_FileManager.getActiveBootSector()->getSectorSizeInBytes();
		while ( entry.getFileTransferInProgressFlagRef() )
		{
			data = m_FileManager.getSelectedFileNextSector( entry );

			for ( unsigned int blockIndex = 0;
					blockIndex < sectorSizeInBytes && midiTrackEventPrimArrIndex < (lenMd * sizeof(MidiTrackEvent));
						blockIndex++ )
			{
				midiTrackEventPrimArr[midiTrackEventPrimArrIndex] = data[blockIndex];
				midiTrackEventPrimArrIndex++;
			}
		}

		// create midi track and push to midi tracks vector
		MidiTrackEvent* midiTrackEvents = reinterpret_cast<MidiTrackEvent*>( midiTrackEventPrimArr );
		MidiTrack midiTrack( cellX, cellY, midiTrackEvents, lenMd, lenBk, m_AxiSramAllocator, true, entry.getFilenameDisplay() );
		m_MidiTracks.push_back( midiTrack );

		// deallocate the primitive array
		m_AxiSramAllocator.free( midiTrackEventPrimArr );

		IMnemonicUiEventListener::PublishEvent(
				MnemonicUiEvent(UiEventType::SCENE_TRACK_FILE_LOADED, nullptr, 0, 0, cellX, cellY) );

		return true;
	}

	return false;
}
