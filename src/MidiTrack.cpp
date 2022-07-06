#include "MidiTrack.hpp"

#include <string.h>

MidiTrack::MidiTrack (unsigned int cellX, unsigned int cellY, const MidiTrackEvent* const midiTrackEvents,
			const unsigned int lengthInMidiTrackEvents, const unsigned int loopEnd, IAllocator& allocator,
			bool isSaved, const char* filenameDisplay) :
	m_CellX( cellX ),
	m_CellY( cellY ),
	m_MidiTrackEvents( SharedData<MidiTrackEvent>::MakeSharedData(lengthInMidiTrackEvents, &allocator) ),
	m_LengthInMidiTrackEvents( lengthInMidiTrackEvents ),
	m_LoopEndInBlocks( loopEnd ),
	m_MidiTrackEventsIndex( 0 ),
	m_IsSaved( isSaved ),
	m_FilenameDisplay(),
	m_WaitToPlay( false ),
	m_WaitToStop( false ),
	m_IsPlaying( false ),
	m_JustFinished( false )
{
	// copy midi events from temp buffer
	for ( unsigned int midiEventNum = 0; midiEventNum < lengthInMidiTrackEvents; midiEventNum++ )
	{
		m_MidiTrackEvents[midiEventNum] = midiTrackEvents[midiEventNum];
	}

	if ( filenameDisplay ) strcpy( m_FilenameDisplay, filenameDisplay );
}

MidiTrack::~MidiTrack()
{
}

bool MidiTrack::waitForLoopStartOrEnd (const unsigned int timeCode)
{
	if ( m_WaitToPlay && timeCode % m_LoopEndInBlocks == 0 )
	{
		m_IsPlaying = true;
		m_WaitToPlay = false;
		return true;
	}
	else if ( m_WaitToStop && timeCode % m_LoopEndInBlocks == 0 )
	{
		m_IsPlaying = false;
		m_WaitToStop = false;
	}

	return false;
}

void MidiTrack::addMidiEventsAtTimeCode( const unsigned int timeCode, std::vector<MidiEvent>& midiEventOutputVector )
{
	// skip to upcoming midi track event
	while ( m_MidiTrackEvents[m_MidiTrackEventsIndex].m_TimeCode < timeCode % m_LoopEndInBlocks
			&& m_MidiTrackEventsIndex != m_LengthInMidiTrackEvents - 1 )
	{
		m_MidiTrackEventsIndex = ( m_MidiTrackEventsIndex + 1 ) % m_LengthInMidiTrackEvents;
	}

	// add all midi track events with the current time code to queue
	while ( m_MidiTrackEvents[m_MidiTrackEventsIndex].m_TimeCode == timeCode % m_LoopEndInBlocks
			&& m_LengthInMidiTrackEvents > 1 )
	{
		midiEventOutputVector.push_back( m_MidiTrackEvents[m_MidiTrackEventsIndex].m_MidiEvent );

		m_MidiTrackEventsIndex = ( m_MidiTrackEventsIndex + 1 ) % m_LengthInMidiTrackEvents;
	}

	// if we've processed the last midi track event, return to the beginning
	if ( m_MidiTrackEventsIndex == m_LengthInMidiTrackEvents - 1 )
	{
		m_MidiTrackEventsIndex = 0;
	}
}

void MidiTrack::play (bool immediately)
{
	if ( immediately )
	{
		m_IsPlaying = true;
		m_WaitToPlay = false;
	}
	else
	{
		m_WaitToPlay = true;
	}
	m_WaitToStop = false;
	m_JustFinished = false;
}

void MidiTrack::stop (bool immediately)
{
	if ( immediately )
	{
		m_IsPlaying = false;
		m_WaitToStop = false;
	}
	else
	{
		m_WaitToStop = true;
	}
	m_WaitToPlay = false;
	m_JustFinished = true;
}

void MidiTrack::setIsSaved (const char* filenameDisplay)
{
	if ( filenameDisplay )
	{
		m_IsSaved = true;
		strcpy( m_FilenameDisplay, filenameDisplay );
	}
	else
	{
		m_IsSaved = false;
	}
}
