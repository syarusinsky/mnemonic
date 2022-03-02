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
	RECORDING = 2
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

		void onMidiEvent (const MidiEvent& midiEvent) override; // TODO this function saves the midi events to a 'track' if recording
		std::vector<MidiEvent>& getMidiEventsToSendVec() { return m_MidiEventsToSend; }

	private:
		IAllocator 			m_AxiSramAllocator;
		Fat16FileManager 		m_FileManager;

		unsigned int 			m_TransportProgress;

		std::vector<AudioTrack> 	m_AudioTracks;

		uint16_t* 			m_DecompressedBuffer; // for holding decompressed audio buffers for all audio tracks

		unsigned int 			m_MasterClockCount;
		unsigned int 			m_CurrentMaxLoopCount; // master clock resets after reaching this amount

		std::vector<MidiTrack> 		m_MidiTracks;

		std::vector<MidiEvent> 		m_MidiEventsToSend; // TODO this is a vector of all midi events at a time code to be sent over usart

		MidiRecordingState 		m_RecordingMidiState; // TODO the state of the midi recording
		MidiTrackEvent* const		m_TempMidiTrackEvents; // TODO a buffer for recording midi clips
		unsigned int 			m_TempMidiTrackEventsIndex; // TODO an index into the current temp midi track recording
		unsigned int 			m_TempMidiTrackEventsNumEvents; // TODO the number of events in the temp midi track recording
		unsigned int 			m_TempMidiTrackEventsLoopEnd; // TODO the ending loop count for the midi track

		void loadFile (unsigned int index);

		void startRecordingMidiTrack(); // TODO probably need to assign a 'cell'
		void endRecordingMidiTrack();

		void enterFileExplorer();

		const Fat16Entry* lookForOtherChannel (const char* filenameDisplay); // for looking for other stereo channel
};

#endif // MNEMONICAUDIOMANAGER_HPP
