#ifndef MNEMONICAUDIOMANAGER_HPP
#define MNEMONICAUDIOMANAGER_HPP

/*************************************************************************
 * The MnemonicAudioManager is responsible for managing audio and midi
 * streams for mnemonic. This includes handling the timing between each
 * of the 'tracks'.
*************************************************************************/

#include "AudioTrack.hpp"
#include "MidiTrack.hpp"
#include "MnemonicConstants.hpp"
#include "IBufferCallback.hpp"
#include "IMidiEventListener.hpp"
#include "Fat16FileManager.hpp"
#include "IMnemonicParameterEventListener.hpp"
#include "IAllocator.hpp"

class IStorageMedia;

enum class MidiRecordingState : unsigned int
{
	NOT_RECORDING = 0,
	WAITING_TO_RECORD = 1,
	RECORDING = 2,
	JUST_FINISHED_RECORDING = 3
};

enum class Directory : unsigned int
{
	ROOT = 0,
	AUDIO = 1,
	MIDI = 2,
	SCENE = 3
};

class MnemonicAudioManager : public IBufferCallback<int16_t, true>, public IMnemonicParameterEventListener, public IMidiEventListener
{
	public:
		MnemonicAudioManager (IStorageMedia& sdCard, uint8_t* axiSramPtr, unsigned int axiSramSizeInBytes);
		~MnemonicAudioManager() override;

		void publishUiEvents(); // updates the periodic ui events

		void verifyFileSystem(); // should be called on boot up at the very least

		void call (int16_t* writeBufferL, int16_t* writeBufferR) override;

		void onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent) override;

		void onMidiEvent (const MidiEvent& midiEvent) override;
		std::vector<MidiEvent>& getMidiEventsToSendVec() { return m_MidiEventsToSend; }

	private:
		IAllocator 			m_AxiSramAllocator;
		Fat16FileManager 		m_FileManager;

		Directory 			m_CurrentDirectory;

		unsigned int 			m_TransportProgress;

		std::vector<AudioTrack> 	m_AudioTracks;

		uint16_t* 			m_DecompressedBuffer; // for holding decompressed audio buffers for all audio tracks

		unsigned int 			m_MasterClockCount;
		unsigned int 			m_CurrentMaxLoopCount; // master clock resets after reaching this amount

		std::vector<MidiTrack> 		m_MidiTracks;

		std::vector<MidiEvent> 		m_MidiEventsToSend; // a vector of all midi events at a time code to be sent over usart

		MidiRecordingState 		m_RecordingMidiState;
		MidiTrackEvent* const		m_TempMidiTrackEvents;
		unsigned int 			m_TempMidiTrackEventsIndex;
		unsigned int 			m_TempMidiTrackEventsNumEvents;
		unsigned int 			m_TempMidiTrackEventsLoopEnd;
		unsigned int 			m_TempMidiTrackCellX;
		unsigned int 			m_TempMidiTrackCellY;

		void resetLoopingInfo();

		void playOrStopTrack (unsigned int cellX, unsigned int cellY, bool play);

		bool goToDirectory (const Directory& directory); // returns false if directory not found, true if successful

		void loadFile (unsigned int cellX, unsigned int cellY, unsigned int index);
		void unloadFile (unsigned int cellX, unsigned int cellY);

		void startRecordingMidiTrack (unsigned int cellX, unsigned int cellY);
		void endRecordingMidiTrack (unsigned int cellX, unsigned int cellY);
		void saveMidiRecording (unsigned int cellX, unsigned int cellY, const char* nameWithoutExt);

		void saveScene (const char* nameWithoutExt);

		uint8_t* enterFileExplorerHelper (const char* extension, unsigned int& numEntries);
		void enterFileExplorer (bool filterMidiFilesInstead = false); // normally filters b12 files

		Fat16Entry* lookForOtherChannel (const char* filenameDisplay); // for looking for other stereo channel
};

#endif // MNEMONICAUDIOMANAGER_HPP
