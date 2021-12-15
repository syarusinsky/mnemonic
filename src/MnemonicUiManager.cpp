#include "MnemonicUiManager.hpp"

#include "Graphics.hpp"

// TODO remove this after testing
#include <iostream>

MnemonicUiManager::MnemonicUiManager (unsigned int width, unsigned int height, const CP_FORMAT& format) :
	Surface( width, height, format ),
	m_Effect1PotCurrentParam( PARAM_CHANNEL::TEST_1 ),
	m_Effect2PotCurrentParam( PARAM_CHANNEL::TEST_2 ),
	m_Effect3PotCurrentParam( PARAM_CHANNEL::TEST_3 ),
	m_Effect1PotCached( 0.0f ),
	m_Effect2PotCached( 0.0f ),
	m_Effect3PotCached( 0.0f ),
	m_Effect1PotLocked( false ),
	m_Effect2PotLocked( false ),
	m_Effect3PotLocked( false ),
	m_Pot1StabilizerBuf{ 0.0f },
	m_Pot2StabilizerBuf{ 0.0f },
	m_Pot3StabilizerBuf{ 0.0f },
	m_Pot1StabilizerIndex( 0 ),
	m_Pot2StabilizerIndex( 0 ),
	m_Pot3StabilizerIndex( 0 ),
	m_Pot1StabilizerValue( 0.0f ),
	m_Pot2StabilizerValue( 0.0f ),
	m_Pot3StabilizerValue( 0.0f ),
	m_Pot1StabilizerCachedPer( 0.0f ),
	m_Pot2StabilizerCachedPer( 0.0f ),
	m_Pot3StabilizerCachedPer( 0.0f ),
	m_Effect1BtnState( BUTTON_STATE::FLOATING ),
	m_Effect2BtnState( BUTTON_STATE::FLOATING )
{
}

MnemonicUiManager::~MnemonicUiManager()
{
}

void MnemonicUiManager::setFont (Font* font)
{
	m_Graphics->setFont( font );
}

void MnemonicUiManager::draw()
{
	m_Graphics->setColor( true );
	m_Graphics->fill();
}

void MnemonicUiManager::onPotEvent (const PotEvent& potEvent)
{
	POT_CHANNEL channel = static_cast<POT_CHANNEL>( potEvent.getChannel() );
	float percentage = potEvent.getPercentage();

	float* potStabilizerBuf = nullptr;
	unsigned int* potStabilizerIndex = nullptr;
	float* potStabilizerValue = nullptr;
	float* potStabilizerCachedPer = nullptr;
	bool* lockedStatus = nullptr;
	float* lockCachedVal = nullptr;
	PARAM_CHANNEL potAssignment = PARAM_CHANNEL::NULL_PARAM;

	switch ( channel )
	{
		case POT_CHANNEL::EFFECT1:
			potStabilizerBuf = m_Pot1StabilizerBuf;
			potStabilizerIndex = &m_Pot1StabilizerIndex;
			potStabilizerValue = &m_Pot1StabilizerValue;
			potStabilizerCachedPer = &m_Pot1StabilizerCachedPer;
			lockedStatus = &m_Effect1PotLocked;
			lockCachedVal = &m_Effect1PotCached;
			potAssignment = m_Effect1PotCurrentParam;

			break;
		case POT_CHANNEL::EFFECT2:
			potStabilizerBuf = m_Pot2StabilizerBuf;
			potStabilizerIndex = &m_Pot2StabilizerIndex;
			potStabilizerValue = &m_Pot2StabilizerValue;
			potStabilizerCachedPer = &m_Pot2StabilizerCachedPer;
			lockedStatus = &m_Effect2PotLocked;
			lockCachedVal = &m_Effect2PotCached;
			potAssignment = m_Effect2PotCurrentParam;

			break;
		case POT_CHANNEL::EFFECT3:
			potStabilizerBuf = m_Pot3StabilizerBuf;
			potStabilizerIndex = &m_Pot3StabilizerIndex;
			potStabilizerValue = &m_Pot3StabilizerValue;
			potStabilizerCachedPer = &m_Pot3StabilizerCachedPer;
			lockedStatus = &m_Effect3PotLocked;
			lockCachedVal = &m_Effect3PotCached;
			potAssignment = m_Effect3PotCurrentParam;

			break;
		default:
			break;
	}

	// stabilize the potentiometer value by averaging all the values in the stabilizer buffers
	float averageValue = percentage;
	for ( unsigned int index = 0; index < MNEMONIC_POT_STABIL_NUM; index++ )
	{
		averageValue += potStabilizerBuf[index];
	}
	averageValue = averageValue / ( static_cast<float>(MNEMONIC_POT_STABIL_NUM) + 1.0f );

	// only if the current value breaks our 'hysteresis' do we actually set a new pot value
	if ( *potStabilizerValue != averageValue )
	{
		*potStabilizerValue = averageValue;

		if ( ! *lockedStatus ) // if not locked
		{
			bool menuThreshBroken = this->hasBrokenMenuChangeThreshold( *potStabilizerValue, *potStabilizerCachedPer );
			this->sendParamEventFromEffectPot( potAssignment, *potStabilizerValue, menuThreshBroken );
		}
		else // if locked, update locked status
		{
			if ( this->hasBrokenLock(*lockedStatus, *lockCachedVal, *potStabilizerValue) )
			{
				bool menuThreshBroken = this->hasBrokenMenuChangeThreshold( *potStabilizerValue, *potStabilizerCachedPer );
				this->sendParamEventFromEffectPot( potAssignment, *potStabilizerValue, menuThreshBroken );
			}
		}
	}

	// write value to buffer and increment index
	potStabilizerBuf[*potStabilizerIndex] = percentage;
	*potStabilizerIndex = ( *potStabilizerIndex + 1 ) % MNEMONIC_POT_STABIL_NUM;
}

void MnemonicUiManager::sendParamEventFromEffectPot (PARAM_CHANNEL param, float val, bool menuThreshBroken)
{
	switch ( param )
	{
		case PARAM_CHANNEL::TEST_1:
			std::cout << "Effect pot 1: " << std::to_string(val) << std::endl;

			break;
		case PARAM_CHANNEL::TEST_2:
			std::cout << "Effect pot 2: " << std::to_string(val) << std::endl;

			break;
		case PARAM_CHANNEL::TEST_3:
			std::cout << "Effect pot 3: " << std::to_string(val) << std::endl;

			break;
		default:
			break;
	}
}

bool MnemonicUiManager::hasBrokenLock (bool& potLockedVal, float& potCachedVal, float newPotVal)
{
	bool hasBrokenLowerRange  = newPotVal <= ( potCachedVal - m_PotChangeThreshold );
	bool hasBrokenHigherRange = newPotVal >= ( potCachedVal + m_PotChangeThreshold );

	if ( potLockedVal && (hasBrokenHigherRange || hasBrokenLowerRange) ) // if locked but broke threshold
	{
		potLockedVal = false;
		return true;
	}

	return false;
}

bool MnemonicUiManager::hasBrokenMenuChangeThreshold (float newVal, float cachedVal)
{
	if ( std::abs(newVal - cachedVal) > MNEMONIC_POT_MENU_CHANGE_THRESH )
	{
		return true;
	}

	return false;
}

void MnemonicUiManager::processEffect1Btn (bool pressed)
{
	static BUTTON_STATE prevState = BUTTON_STATE::FLOATING;
	this->updateButtonState( m_Effect1BtnState, pressed );

	if ( prevState != m_Effect1BtnState )
	{
		prevState = m_Effect1BtnState;

		// this button event is then processed by this class' onButtonEvent with logic per menu
		IButtonEventListener::PublishEvent( ButtonEvent(prevState, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT1)) );
	}
}

void MnemonicUiManager::processEffect2Btn (bool pressed)
{
	static BUTTON_STATE prevState = BUTTON_STATE::FLOATING;
	this->updateButtonState( m_Effect2BtnState, pressed );

	if ( prevState != m_Effect2BtnState )
	{
		prevState = m_Effect2BtnState;

		// this button event is then processed by this class' onButtonEvent with logic per menu
		IButtonEventListener::PublishEvent( ButtonEvent(prevState, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT2)) );
	}
}

void MnemonicUiManager::updateButtonState (BUTTON_STATE& buttonState, bool pressed)
{
	if ( pressed )
	{
		if ( buttonState == BUTTON_STATE::FLOATING || buttonState == BUTTON_STATE::RELEASED )
		{
			buttonState = BUTTON_STATE::PRESSED;
		}
		else
		{
			buttonState = BUTTON_STATE::HELD;
		}
	}
	else
	{
		if ( buttonState == BUTTON_STATE::PRESSED || buttonState == BUTTON_STATE::HELD )
		{
			buttonState = BUTTON_STATE::RELEASED;
		}
		else
		{
			buttonState = BUTTON_STATE::FLOATING;
		}
	}
}

void MnemonicUiManager::onButtonEvent (const ButtonEvent& buttonEvent)
{
	static bool ignoreNextReleaseEffect1 = false;
	static bool ignoreNextReleaseEffect2 = false;

	if ( buttonEvent.getButtonState() == BUTTON_STATE::RELEASED )
	{
		if ( buttonEvent.getChannel() == static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT1) )
		{
			if ( ignoreNextReleaseEffect1 )
			{
				ignoreNextReleaseEffect1 = false;
			}
			else
			{
				if ( m_Effect2BtnState == BUTTON_STATE::HELD ) // double button press
				{
					ignoreNextReleaseEffect2 = true;
					this->handleDoubleButtonPress();
				}
				else // single button press
				{
					this->handleEffect1SinglePress();
				}
			}
		}
		else if ( buttonEvent.getChannel() == static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT2) )
		{
			if ( ignoreNextReleaseEffect2 )
			{
				ignoreNextReleaseEffect2 = false;
			}
			else
			{
				if ( m_Effect1BtnState == BUTTON_STATE::HELD ) // double button press
				{
					ignoreNextReleaseEffect1 = true;
					this->handleDoubleButtonPress();
				}
				else // single button press
				{
					this->handleEffect2SinglePress();
				}
			}
		}
	}
}

void MnemonicUiManager::handleEffect1SinglePress()
{
	std::cout << "Effect 1 Button Pressed" << std::endl;
}

void MnemonicUiManager::handleEffect2SinglePress()
{
	std::cout << "Effect 2 Button Pressed" << std::endl;
}

void MnemonicUiManager::handleDoubleButtonPress()
{
	std::cout << "Double Buttons Pressed" << std::endl;
}
