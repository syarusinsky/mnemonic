#include "MidiTrack.hpp"

MidiTrack::MidiTrack (const MidiTrackEvent* const midiTrackEvents, const unsigned int lengthInMidiTrackEvents, const unsigned int loopEnd) :
	m_MidiTrackEvents( midiTrackEvents ),
	m_LengthInMidiTrackEvents( lengthInMidiTrackEvents ),
	m_LoopEndInBlocks( loopEnd ),
	m_MidiTrackEventsIndex( 0 )
{
}

MidiTrack::~MidiTrack()
{
}

const MidiTrackEvent* const MidiTrack::getMidiTrackEvents()
{
	return m_MidiTrackEvents;
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
