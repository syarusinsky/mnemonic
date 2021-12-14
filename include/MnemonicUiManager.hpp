#ifndef MNEMONICUIMANAGER_HPP
#define MNEMONICUIMANAGER_HPP

/*************************************************************************
 * The MnemonicUiManager is a SIGL Surface that receives events when
 * the mnemonic changes a parameter and refreshes the contents of the frame
 * buffer accordingly. In order to be efficient as possible it also
 * sends screen refresh events indicating the part of the screen that
 * needs to be refreshed. This way the actual lcd HAL can send only the
 * relevant data, instead of an entire frame buffer.
*************************************************************************/

#include "Surface.hpp"

#include <stdint.h>

#include "MnemonicConstants.hpp"

class Font;

class MnemonicUiManager : public Surface
{
	public:
		MnemonicUiManager (unsigned int width, unsigned int height, const CP_FORMAT& format);
		~MnemonicUiManager() override;

		void setFont (Font* font);

		void draw() override;
};

#endif // MNEMONICUIMANAGER_HPP
