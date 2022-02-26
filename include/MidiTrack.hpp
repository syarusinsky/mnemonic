#ifndef MIDITRACK_HPP
#define MIDITRACK_HPP

/*************************************************************************
 * A MidiTrack defines a stream of midi events with time codes to be
 * played in a loop.
*************************************************************************/

#include "IMidiEventListener.hpp"
#include <vector>

struct MidiTrackEvent
{
	MidiEvent 	m_MidiEvent;
	unsigned int 	m_TimeCode;
};

class MidiTrack
{
	public:
		MidiTrack (const MidiTrackEvent* const midiTrackEvents, const unsigned int lengthInMidiTrackEvents, const unsigned int loopEnd);
		~MidiTrack();

		void addMidiEventsAtTimeCode( const unsigned int timeCode, std::vector<MidiEvent>& midiEventOutputVector );

		const MidiTrackEvent* const getMidiTrackEvents();

	private:
		const MidiTrackEvent* const	m_MidiTrackEvents;
		const unsigned int 		m_LengthInMidiTrackEvents;
		const unsigned int 		m_LoopEndInBlocks;

		unsigned int 			m_MidiTrackEventsIndex;
};

#endif // MIDITRACK_HPP
