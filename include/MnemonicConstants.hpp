#ifndef MNEMONICCONSTANTS_HPP
#define MNEMONICCONSTANTS_HPP

#include <limits>

#define MNEMONIC_COLOR_INACTIVE 150, 150, 150
#define MNEMONIC_COLOR_TRANSPORT_ACTIVE 60, 135, 255

constexpr unsigned int MNEMONIC_MAX_MIDI_TRACK_EVENTS = 1000; // the max number of midi events able to record for a midi track

constexpr unsigned int MNEMONIC_POT_STABIL_NUM = 50; // pot stabilization stuff
constexpr float MNEMONIC_POT_MENU_CHANGE_THRESH = 0.5f; // threshold to break for a status submenu change

constexpr unsigned int MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE = 1000;
constexpr unsigned int MNEMONIC_UI_EVENT_QUEUE_SIZE = 10;

enum class PARAM_CHANNEL : unsigned int
{
	NULL_PARAM 		= 0,
	ENTER_FILE_EXPLORER 	= 1,
	LOAD_FILE 		= 2,
	START_MIDI_RECORDING 	= 3,
	END_MIDI_RECORDING 	= 4
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
