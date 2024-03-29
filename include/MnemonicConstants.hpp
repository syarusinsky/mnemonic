#ifndef MNEMONICCONSTANTS_HPP
#define MNEMONICCONSTANTS_HPP

#include <limits>

#define MNEMONIC_COLOR_INACTIVE 			0, 0, 0
#define MNEMONIC_COLOR_TRANSPORT_ACTIVE 		60, 135, 255
#define MNEMONIC_COLOR_LOADING_FILE 			255, 255, 0
#define MNEMONIC_COLOR_AUDIO_LOOPS_ROW_1_NOT_PLAYING 	50, 150, 50
#define MNEMONIC_COLOR_AUDIO_LOOPS_ROW_1_PLAYING 	50, 255, 50
#define MNEMONIC_COLOR_AUDIO_LOOPS_ROW_2_NOT_PLAYING 	100, 50, 100
#define MNEMONIC_COLOR_AUDIO_LOOPS_ROW_2_PLAYING 	255, 50, 255
#define MNEMONIC_COLOR_AUDIO_ONESHOTS_NOT_PLAYING 	50, 50, 150
#define MNEMONIC_COLOR_AUDIO_ONESHOTS_PLAYING 		50, 50, 255
#define MNEMONIC_COLOR_MIDI_RECORDING 			255, 0, 0
#define MNEMONIC_COLOR_MIDI_NOT_PLAYING 		100, 100, 150
#define MNEMONIC_COLOR_MIDI_PLAYING 			100, 100, 255

enum class MNEMONIC_ROW : unsigned int
{
	TRANSPORT = 0,
	AUDIO_LOOPS_1 = 1,
	AUDIO_LOOPS_2 = 2,
	AUDIO_ONESHOTS = 3,
	MIDI_CHAN_1_LOOPS = 4,
	MIDI_CHAN_2_LOOPS = 5,
	MIDI_CHAN_3_LOOPS = 6,
	MIDI_CHAN_4_LOOPS = 7,
};

constexpr unsigned int MNEMONIC_NEOTRELLIS_ROWS = 8;
constexpr unsigned int MNEMONIC_NEOTRELLIS_COLS = 8;

constexpr unsigned int MNEMONIC_MAX_MIDI_TRACK_EVENTS = 1000; // the max number of midi events able to record for a midi track

constexpr unsigned int MNEMONIC_POT_STABIL_NUM = 50; // pot stabilization stuff
constexpr float MNEMONIC_POT_MENU_CHANGE_THRESH = 0.5f; // threshold to break for a status submenu change

constexpr unsigned int MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE = 1000;
constexpr unsigned int MNEMONIC_UI_EVENT_QUEUE_SIZE = 10;

constexpr const char* MNEMONIC_SCENE_VERSION = "1.0.0";

enum class PARAM_CHANNEL : unsigned int
{
	NULL_PARAM 		= 0,
	LOAD_AUDIO_RECORDING,
	LOAD_FILE,
	UNLOAD_FILE,
	PLAY_OR_STOP_TRACK,
	START_MIDI_RECORDING,
	END_MIDI_RECORDING,
	SAVE_MIDI_RECORDING,
	LOAD_MIDI_RECORDING,
	SAVE_SCENE,
	LOAD_SCENE,
	ACTIVE_MIDI_CHANNEL,
	DELETE_FILE,
	CONFIRM_DELETE_FILE
};

enum class POT_CHANNEL : unsigned int
{
	EFFECT1 = 0,
	EFFECT2 = 1,
	EFFECT3 = 2
};

enum class BUTTON_CHANNEL : unsigned int
{
	EFFECT1 = 0,
	EFFECT2 = 1
};

#endif // MNEMONICCONSTANTS_HPP
