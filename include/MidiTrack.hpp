#ifndef MIDITRACK_HPP
#define MIDITRACK_HPP

/*************************************************************************
 * A MidiTrack defines a stream of midi events with time codes to be
 * played in a loop.
*************************************************************************/

#include "IMidiEventListener.hpp"

struct MidiTrackEvent
{
	MidiEvent 	m_MidiEvent;
	unsigned int 	m_TimeCode;
};

class MidiTrack
{
	public:
		MidiTrack (const MidiTrackEvent* midiTrackEvents, const unsigned int lengthInMidiTrackEvents);
		~MidiTrack();

	private:
		const MidiTrackEvent* 	m_MidiTrackEvents;
		const unsigned int 	m_LengthInMidiTrackEvents;
};

#endif // MIDITRACK_HPP
