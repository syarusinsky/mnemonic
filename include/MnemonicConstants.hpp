#ifndef MNEMONICCONSTANTS_HPP
#define MNEMONICCONSTANTS_HPP

#include <limits>

constexpr unsigned int MNEMONIC_POT_STABIL_NUM = 50; // pot stabilization stuff
constexpr float MNEMONIC_POT_MENU_CHANGE_THRESH = 0.5f; // threshold to break for a status submenu change

constexpr unsigned int MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE = 1000;
constexpr unsigned int MNEMONIC_UI_EVENT_QUEUE_SIZE = 10;

enum class PARAM_CHANNEL : unsigned int
{
	NULL_PARAM 	= 0,
	LOAD_FILE 	= 1
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
