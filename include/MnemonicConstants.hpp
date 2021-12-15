#ifndef MNEMONICCONSTANTS_HPP
#define MNEMONICCONSTANTS_HPP

#include <limits>

constexpr unsigned int MNEMONIC_POT_STABIL_NUM = 50; // pot stabilization stuff
constexpr float MNEMONIC_POT_MENU_CHANGE_THRESH = 0.5f; // threshold to break for a status submenu change

enum class PARAM_CHANNEL : unsigned int
{
	NULL_PARAM 	= 0,
	TEST_1 		= 1,
	TEST_2 		= 2,
	TEST_3 		= 3
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
