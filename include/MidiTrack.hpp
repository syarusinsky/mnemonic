#ifndef MIDITRACK_HPP
#define MIDITRACK_HPP

/*************************************************************************
 * A MidiTrack defines a stream of midi events with time codes to be
 * played in a loop.
*************************************************************************/

#include "IMidiEventListener.hpp"
#include "SharedData.hpp"
#include <vector>

class IAllocator;

struct MidiTrackEvent
{
	MidiEvent 	m_MidiEvent;
	unsigned int 	m_TimeCode;
};

class MidiTrack
{
	public:
		MidiTrack (unsigned int cellX, unsigned int cellY, const MidiTrackEvent* const midiTrackEvents,
				const unsigned int lengthInMidiTrackEvents, const unsigned int loopEnd, IAllocator& allocator);
		~MidiTrack();

		unsigned int getCellX() const { return m_CellX; }
		unsigned int getCellY() const { return m_CellY; }

		SharedData<MidiTrackEvent> getData() { return m_MidiTrackEvents; }

		unsigned int getLengthInMidiTrackEvents() { return m_LengthInMidiTrackEvents; }
		unsigned int getLoopEndInBlocks() { return m_LoopEndInBlocks; }

		void play (bool immediately = false); // only start when last loop is complete, unless immediatly = true
		void stop (bool immediately = false);

		bool isPlaying() const { return m_IsPlaying; }

		bool justFinished() { const bool justFinished = m_JustFinished; m_JustFinished = false; return justFinished; }

		void waitForLoopStartOrEnd (const unsigned int timeCode);
		void addMidiEventsAtTimeCode (const unsigned int timeCode, std::vector<MidiEvent>& midiEventOutputVector);

	private:
		unsigned int 			m_CellX;
		unsigned int 			m_CellY;

		SharedData<MidiTrackEvent>	m_MidiTrackEvents;
		unsigned int 			m_LengthInMidiTrackEvents;
		unsigned int 			m_LoopEndInBlocks;

		unsigned int 			m_MidiTrackEventsIndex;

		bool 				m_WaitToPlay; // both to ensure stop and start at the beginning of the loops
		bool 				m_WaitToStop;

		bool 				m_IsPlaying;

		bool 				m_JustFinished;
};

#endif // MIDITRACK_HPP
