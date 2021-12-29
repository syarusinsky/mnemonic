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
#include <string>

#include "MnemonicConstants.hpp"
#include "IMnemonicUiEventListener.hpp"
#include "IPotEventListener.hpp"
#include "IButtonEventListener.hpp"

class Font;

class MnemonicUiManager : public Surface, public IPotEventListener, public IButtonEventListener, public IMnemonicUiEventListener
{
	public:
		MnemonicUiManager (unsigned int width, unsigned int height, const CP_FORMAT& format);
		~MnemonicUiManager() override;

		void setFont (Font* font);

		void draw() override;

		void onPotEvent (const PotEvent& potEvent) override;

		void onButtonEvent (const ButtonEvent& buttonEvent) override;

		void processEffect1Btn (bool pressed);
		void processEffect2Btn (bool pressed);

		void onMnemonicUiEvent (const MnemonicUiEvent& event) override;

	private:
		// the parameters each effect pot is currently assigned to
		PARAM_CHANNEL 	m_Effect1PotCurrentParam;
		PARAM_CHANNEL 	m_Effect2PotCurrentParam;
		PARAM_CHANNEL 	m_Effect3PotCurrentParam;

		// pot cached values for parameter thresholds (so preset parameters don't change unless moved by a certain amount)
		const float     m_PotChangeThreshold = 0.2f; // the pot value needs to break out of this threshold to be applied
		float 		m_Effect1PotCached;
		float 		m_Effect2PotCached;
		float 		m_Effect3PotCached;
		// these keep track of whether the given pots are 'locked' by the threshold when switching presets
		bool 		m_Effect1PotLocked;
		bool 		m_Effect2PotLocked;
		bool 		m_Effect3PotLocked;

		// potentiometer stabilization
		float 		m_Pot1StabilizerBuf[MNEMONIC_POT_STABIL_NUM];
		float 		m_Pot2StabilizerBuf[MNEMONIC_POT_STABIL_NUM];
		float 		m_Pot3StabilizerBuf[MNEMONIC_POT_STABIL_NUM];
		unsigned int 	m_Pot1StabilizerIndex;
		unsigned int 	m_Pot2StabilizerIndex;
		unsigned int 	m_Pot3StabilizerIndex;
		float 		m_Pot1StabilizerValue; // we use this as the actual value to send
		float 		m_Pot2StabilizerValue;
		float 		m_Pot3StabilizerValue;
		float 		m_Pot1StabilizerCachedPer; // cached percentage for switching between status submenus
		float 		m_Pot2StabilizerCachedPer;
		float 		m_Pot3StabilizerCachedPer;

		// button state handling
		BUTTON_STATE 	m_Effect1BtnState;
		BUTTON_STATE 	m_Effect2BtnState;

		void sendParamEventFromEffectPot (PARAM_CHANNEL param, float val, bool menuThreshBroken);

		void updateButtonState (BUTTON_STATE& buttonState, bool pressed); // note: buttonState is an output variable
		// logic to handle button presses (in this case, releases) for each menu case
		void handleEffect1SinglePress();
		void handleEffect2SinglePress();
		void handleDoubleButtonPress();

		bool hasBrokenLock (bool& potLockedVal, float& potCachedVal, float newPotVal);
		bool hasBrokenMenuChangeThreshold (float newVal, float cachedVal);

		void displayErrorMessage (const std::string& errorMessage);
};

#endif // MNEMONICUIMANAGER_HPP
