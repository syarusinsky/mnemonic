#ifndef MIDITRACK_HPP
#define MIDITRACK_HPP

/*************************************************************************
 * A MidiTrack defines a stream of midi events with time codes to be
 * played in a loop.
*************************************************************************/

#include "IMidiEventListener.hpp"
#include "SharedData.hpp"
#include "Fat16Entry.hpp"
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
				const unsigned int lengthInMidiTrackEvents, const unsigned int loopEnd, IAllocator& allocator,
				bool isSaved = false, const char* filenameDisplay = nullptr);
		~MidiTrack();

		unsigned int getCellX() const { return m_CellX; }
		unsigned int getCellY() const { return m_CellY; }

		SharedData<MidiTrackEvent> getData() { return m_MidiTrackEvents; }

		unsigned int getLengthInMidiTrackEvents() const { return m_LengthInMidiTrackEvents; }
		unsigned int getLoopEndInBlocks() const { return m_LoopEndInBlocks; }

		void play (bool immediately = false); // only start when last loop is complete, unless immediatly = true
		void stop (bool immediately = false);

		void setIsSaved (const char* filenameDisplay);
		bool isSaved() const { return m_IsSaved; }

		const char* getFilenameDisplay() const { return m_FilenameDisplay; }

		bool isPlaying() const { return m_IsPlaying; }

		bool justFinished() { const bool justFinished = m_JustFinished; m_JustFinished = false; return justFinished; }

		bool waitForLoopStartOrEnd (const unsigned int timeCode); // returns true when just started
		void addMidiEventsAtTimeCode (const unsigned int timeCode, std::vector<MidiEvent>& midiEventOutputVector);

	private:
		unsigned int 			m_CellX;
		unsigned int 			m_CellY;

		SharedData<MidiTrackEvent>	m_MidiTrackEvents;
		unsigned int 			m_LengthInMidiTrackEvents;
		unsigned int 			m_LoopEndInBlocks;

		unsigned int 			m_MidiTrackEventsIndex;

		bool 				m_IsSaved; // whether or not the midi file is saved in the file system
		char 				m_FilenameDisplay[FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2];

		bool 				m_WaitToPlay; // both to ensure stop and start at the beginning of the loops
		bool 				m_WaitToStop;

		bool 				m_IsPlaying;

		bool 				m_JustFinished;
};

#endif // MIDITRACK_HPP
