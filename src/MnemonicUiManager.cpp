#include "MnemonicUiManager.hpp"

#include <cstring>
#include "Graphics.hpp"
#include "IMnemonicParameterEventListener.hpp"
#include "IMnemonicLCDRefreshEventListener.hpp"
#include "Font.hpp"
#include "mnemonic.h"

constexpr unsigned int SETTINGS_NUM_VISIBLE_ENTRIES = 6;

void onNeotrellisButtonHelperFunc (NeotrellisListener* listener, NeotrellisInterface* neotrellis, bool keyReleased, uint8_t keyRow, uint8_t keyCol)
{
	listener->onNeotrellisButton( neotrellis, keyReleased, keyRow, keyCol );
}

MnemonicUiManager::MnemonicUiManager (unsigned int width, unsigned int height, const CP_FORMAT& format, NeotrellisInterface* const neotrellis) :
	Surface( width, height, format ),
	m_Neotrellis( neotrellis ),
	m_MainImage( mnemonic_data ),
	m_Font( nullptr ),
	m_AudioFileEntries(),
	m_MidiFileEntries(),
	m_SceneFileEntries(),
	m_FileEntriesToUse( &m_AudioFileEntries ),
	m_AudioFileMenuModel( SETTINGS_NUM_VISIBLE_ENTRIES ),
	m_MidiFileMenuModel( SETTINGS_NUM_VISIBLE_ENTRIES ),
	m_SceneFileMenuModel( SETTINGS_NUM_VISIBLE_ENTRIES ),
	m_MenuModelToUse( &m_AudioFileMenuModel ),
	m_FileDeleteMode( false ),
	m_FileIndexToDelete( 0 ),
	m_StringEditModel(),
	m_CurrentMenu( MNEMONIC_MENUS::STATUS ),
	m_ActiveMidiChannel( 1 ),
	m_CachedCell(),
	m_CellStates{},
	m_Effect1PotCurrentParam( PARAM_CHANNEL::ACTIVE_MIDI_CHANNEL ),
	m_Effect2PotCurrentParam( PARAM_CHANNEL::NULL_PARAM ),
	m_Effect3PotCurrentParam( PARAM_CHANNEL::NULL_PARAM ),
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
	m_Neotrellis->begin( this );

	for ( unsigned int row = 0; row < m_Neotrellis->getNumRows(); row++ )
	{
		for ( unsigned int col = 0; col < m_Neotrellis->getNumCols(); col++ )
		{
			m_Neotrellis->registerCallback( row, col, onNeotrellisButtonHelperFunc );
			this->setCellStateAndColor( col, row, CELL_STATE::INACTIVE );
		}
	}
}

MnemonicUiManager::~MnemonicUiManager()
{
}

void MnemonicUiManager::setFont (Font* font)
{
	m_Graphics->setFont( font );
	m_Font = font;
}

void MnemonicUiManager::draw()
{
	if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS )
	{
		m_Graphics->setColor( false );
		m_Graphics->fill();

		m_Graphics->setColor( true );
		m_Graphics->drawSprite( 0.0f, 0.0f, m_MainImage );

		if ( (m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD)
				&& (m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD) )
		{
			m_Graphics->drawText( 0.075f, 0.28f, "SCENE:", 1.0f );
			m_Graphics->drawText( 0.075f, 0.39f, "DELETE", 1.0f );
			m_Graphics->drawText( 0.12f, 0.75f, "MIDI:", 1.0f );
			m_Graphics->drawText( 0.075f, 0.88f, "DELETE", 1.0f );
		}
		else if ( m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD )
		{
			m_Graphics->drawText( 0.075f, 0.28f, "SCENE:", 1.0f );
			m_Graphics->drawText( 0.135f, 0.39f, "LOAD", 1.0f );
			m_Graphics->drawText( 0.075f, 0.5f, "AUDIO:", 1.0f );
			m_Graphics->drawText( 0.02f, 0.62f, "LOAD/REM", 1.0f );
			m_Graphics->drawText( 0.12f, 0.75f, "MIDI:", 1.0f );
			m_Graphics->drawText( 0.02f, 0.88f, "LOAD/REM", 1.0f );
		}
		else if ( m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD )
		{
			m_Graphics->drawText( 0.075f, 0.28f, "SCENE:", 1.0f );
			m_Graphics->drawText( 0.135f, 0.39f, "SAVE", 1.0f );
			m_Graphics->drawText( 0.12f, 0.75f, "MIDI:", 1.0f );
			m_Graphics->drawText( 0.135f, 0.88f, "SAVE", 1.0f );
		}
		else
		{
			m_Graphics->drawText( 0.075f, 0.35f, "ACTIVE", 1.0f );
			m_Graphics->drawText( 0.135f, 0.5f, "MIDI", 1.0f );
			m_Graphics->drawText( 0.065f, 0.65f, "CHANNEL", 1.0f );
			m_Graphics->drawBoxFilled( 0.21f, 0.8f, 0.28f, 0.92f );
			m_Graphics->setColor( false );
			char activeChanStr[2] = { 'a', '\0' };
			sprintf( activeChanStr, "%d", m_ActiveMidiChannel );
			m_Graphics->drawText( 0.22f, 0.82f, activeChanStr, 1.0f );

		}

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::FILE_EXPLORER )
	{
		this->drawScrollableMenu( *m_MenuModelToUse, nullptr, *this );
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::STRING_EDIT )
	{
		m_Graphics->setColor( false );
		m_Graphics->fill();

		m_Graphics->setColor( true );
		m_Graphics->drawText( 0.0f, 0.1f, "PRESS CELL TO SAVE", 1.0f );
		m_Graphics->drawText( 0.1f, 0.73f, "EMPTY STRING TO", 1.0f );
		m_Graphics->drawText( 0.35f, 0.88f, "CANCEL", 1.0f );

		const char* string = m_StringEditModel.getString();
		m_Graphics->drawText( 0.3f, 0.4f, string, 1.0f );
		const unsigned int cursorPos = m_StringEditModel.getCursorPos();
		const float charSpan = static_cast<float>( m_Font->getCharacterWidth() ) / this->getFrameBuffer()->getWidth();
		const float startX = 0.3f + ( charSpan * cursorPos );

		if ( m_StringEditModel.getEditMode() ) // in edit mode
		{
			m_Graphics->drawBoxFilled( startX, 0.37f, startX + charSpan, 0.5f );
			char charBeingEdited[] = { string[cursorPos], '\0' };
			m_Graphics->setColor( false );
			m_Graphics->drawText( startX, 0.4f, charBeingEdited, 1.0f );
		}
		else // in hover mode
		{
			m_Graphics->drawLine( startX, 0.5f, startX + charSpan, 0.5f );
		}

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::MIDI_RECORDING_FAILED )
	{
		m_Graphics->setColor( false );
		m_Graphics->fill();

		m_Graphics->setColor( true );
		m_Graphics->drawText( 0.05f, 0.4f, "RECORDING FAILED", 1.0f );

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::SCENE_SAVING_FAILED )
	{
		m_Graphics->setColor( false );
		m_Graphics->fill();

		m_Graphics->setColor( true );
		m_Graphics->drawText( 0.05f, 0.4f, "SAVING FAILED", 1.0f );

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::CONFIRM_DELETE )
	{
		m_Graphics->setColor( false );
		m_Graphics->fill();

		m_Graphics->setColor( true );
		m_Graphics->drawText( 0.17f, 0.3f, "DELETE FILE?", 1.0f );
		m_Graphics->drawLine( 0.17f, 0.4f, 0.8f, 0.4f );
		m_Graphics->drawText( 0.25f, 0.55f, "BTN 1 YES", 1.0f );
		m_Graphics->drawText( 0.25f, 0.7f, "BTN 2  NO", 1.0f );

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
	else
	{
		m_Graphics->setColor( true );
		m_Graphics->fill();

		IMnemonicLCDRefreshEventListener::PublishEvent(
				MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
	}
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
		case PARAM_CHANNEL::ACTIVE_MIDI_CHANNEL:
		{
			bool changed = false;

			if ( val < 0.25f && m_ActiveMidiChannel != 1 ) // midi active channel 1
			{
				m_ActiveMidiChannel = 1;
				changed = true;
			}
			else if ( val >= 0.25f && val < 0.5f && m_ActiveMidiChannel != 2 ) // midi active channel 2
			{
				m_ActiveMidiChannel = 2;
				changed = true;
			}
			else if ( val >= 0.5f && val < 0.75f && m_ActiveMidiChannel != 3 ) // midi active channel 3
			{
				m_ActiveMidiChannel = 3;
				changed = true;
			}
			else if ( val >= 0.75f && m_ActiveMidiChannel != 4 )// midi active channel 4
			{
				m_ActiveMidiChannel = 4;
				changed = true;
			}

			if ( changed )
			{
				if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS )
				{
					this->draw();
				}
				IMnemonicParameterEventListener::PublishEvent(
					MnemonicParameterEvent(0, 0, m_ActiveMidiChannel, static_cast<unsigned int>(param)) );
			}
		}

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

	if ( buttonEvent.getButtonState() == BUTTON_STATE::PRESSED )
	{
		if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS ) this->draw(); // to update help messages
	}
	else if ( buttonEvent.getButtonState() == BUTTON_STATE::RELEASED )
	{
		if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS ) this->draw(); // to update help messages
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
	if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS )
	{
		// do nothing for now
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::FILE_EXPLORER )
	{
		m_MenuModelToUse->reverseCursor();
		this->draw();
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::STRING_EDIT )
	{
		m_StringEditModel.cursorPrev();
		this->draw();
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::CONFIRM_DELETE )
	{
		m_CurrentMenu = MNEMONIC_MENUS::STATUS;
		this->draw();

		IMnemonicParameterEventListener::PublishEvent(
				MnemonicParameterEvent(m_CachedCell.x, m_CachedCell.y, m_FileIndexToDelete,
					static_cast<unsigned int>(PARAM_CHANNEL::CONFIRM_DELETE_FILE)) );
	}
}

void MnemonicUiManager::handleEffect2SinglePress()
{
	if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS )
	{
		// do nothing for now
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::FILE_EXPLORER )
	{
		m_MenuModelToUse->advanceCursor();
		this->draw();
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::STRING_EDIT )
	{
		m_StringEditModel.cursorNext();
		this->draw();
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::CONFIRM_DELETE )
	{
		m_CurrentMenu = MNEMONIC_MENUS::STATUS;
		this->draw();
	}
}

void MnemonicUiManager::handleDoubleButtonPress()
{
	if ( m_CurrentMenu == MNEMONIC_MENUS::STATUS )
	{
		// do nothing for now
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::FILE_EXPLORER )
	{
		// since accessing the file deletion menu is done with a double press, the first double press release will be unintended so we ignore it
		static bool ignoreNextDoublePress = true;
		if ( ! m_FileDeleteMode ) // loading a file
		{
			// set cell states corrrectly
			this->setCellStateAndColor( m_CachedCell.x, m_CachedCell.y, CELL_STATE::NOT_PLAYING );

			// select file in file explorer
			unsigned int index = (*m_FileEntriesToUse)[m_MenuModelToUse->getEntryIndex()].m_Index;
			IMnemonicParameterEventListener::PublishEvent(
					MnemonicParameterEvent(m_CachedCell.x, m_CachedCell.y, index,
						static_cast<unsigned int>(PARAM_CHANNEL::LOAD_FILE)) );

			// return to status menu
			m_CurrentMenu = MNEMONIC_MENUS::STATUS;
			this->draw();
		}
		else if ( ! ignoreNextDoublePress ) // deleting a file
		{
			m_FileIndexToDelete = (*m_FileEntriesToUse)[m_MenuModelToUse->getEntryIndex()].m_Index;
			m_FileDeleteMode = false;

			m_CurrentMenu = MNEMONIC_MENUS::CONFIRM_DELETE;
			this->draw();

			ignoreNextDoublePress = true;
		}
		else // just handling the ignore
		{
			ignoreNextDoublePress = false;
		}
	}
	else if ( m_CurrentMenu == MNEMONIC_MENUS::STRING_EDIT )
	{
		m_StringEditModel.setEditMode( ! m_StringEditModel.getEditMode() );
		this->draw();
	}
}

void MnemonicUiManager::onNeotrellisButton (NeotrellisInterface* neotrellis, bool keyReleased, uint8_t keyRow, uint8_t keyCol)
{
	uint8_t stackedRowNum = neotrellis->getStackedRowNumInMultitrellis( reinterpret_cast<Multitrellis*>(m_Neotrellis) );
	uint8_t stackedColNum = neotrellis->getStackedColNumInMultitrellis( reinterpret_cast<Multitrellis*>(m_Neotrellis) );
	uint8_t keyX = keyCol + ( stackedColNum * NEO_TRELLIS_NUM_COLUMNS );
	uint8_t keyY = keyRow + ( stackedRowNum * NEO_TRELLIS_NUM_ROWS );
	if ( keyReleased && m_CurrentMenu == MNEMONIC_MENUS::STATUS )
	{
		CELL_STATE cellState = this->getCellState( keyX, keyY );
		const MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( keyY );

		if ( row == MNEMONIC_ROW::TRANSPORT )
		{
			// for now only use for loading, saving, and deleteing scenes, maybe in the future scrub audio and midi tracks to this time code?
			if ( (m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD)
					&& (m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD) )
			{
				m_FileDeleteMode = true;
				IMnemonicParameterEventListener::PublishEvent(
						MnemonicParameterEvent(keyX, keyY, 0,
							static_cast<unsigned int>(PARAM_CHANNEL::DELETE_FILE)) );
			}
			else if ( m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD )
			{
				// first unload the currently used cells
				for ( unsigned int x = 0; x < MNEMONIC_NEOTRELLIS_COLS; x++ )
				{
					for ( unsigned int y = 0; y < MNEMONIC_NEOTRELLIS_ROWS; y++ )
					{
						CELL_STATE state = m_CellStates[x][y];
						if ( state == CELL_STATE::NOT_PLAYING
								|| state == CELL_STATE::PLAYING )
						{
							this->setCellStateAndColor( x, y, CELL_STATE::INACTIVE );
							IMnemonicParameterEventListener::PublishEvent(
									MnemonicParameterEvent(x, y, 0,
										static_cast<unsigned int>(PARAM_CHANNEL::UNLOAD_FILE)) );
						}
						else if ( state == CELL_STATE::RECORDING
								|| state == CELL_STATE::LOADING	)
						{
							// TODO probably should display something to the user
							return;
						}
					}
				}

				m_CachedCell.x = keyX;
				m_CachedCell.y = keyY;

				// then signal to load the scene
				IMnemonicParameterEventListener::PublishEvent(
						MnemonicParameterEvent(keyX, keyY, 0,
							static_cast<unsigned int>(PARAM_CHANNEL::LOAD_SCENE)) );
			}
			else if ( m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD )
			{
				// move to string edit menu
				m_StringEditModel.setString( "        " );
				m_CurrentMenu = MNEMONIC_MENUS::STRING_EDIT;
				m_CachedCell.x = keyX;
				m_CachedCell.y = keyY;
				this->draw();
			}
		}
		else if ( (row == MNEMONIC_ROW::AUDIO_LOOPS_1 || row == MNEMONIC_ROW::AUDIO_LOOPS_2 || row == MNEMONIC_ROW::AUDIO_ONESHOTS) )
		{
			if ( (m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD)
					&& (m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD) )
			{
				m_FileDeleteMode = true;
				IMnemonicParameterEventListener::PublishEvent(
						MnemonicParameterEvent(keyX, keyY, 0,
							static_cast<unsigned int>(PARAM_CHANNEL::DELETE_FILE)) );
			}
			else if ( m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD )
			{
				if ( cellState == CELL_STATE::INACTIVE )
				{
					// if inactive, enter file explorer to load a file
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::LOADING );
					m_CachedCell.x = keyX;
					m_CachedCell.y = keyY;
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, 0,
								static_cast<unsigned int>(PARAM_CHANNEL::LOAD_AUDIO_RECORDING)) );
				}
				else if ( cellState == CELL_STATE::NOT_PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::INACTIVE );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, 0,
								static_cast<unsigned int>(PARAM_CHANNEL::UNLOAD_FILE)) );
				}
			}
			else if ( ! (m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD
						|| m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD) )
			{
				if ( cellState == CELL_STATE::NOT_PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::PLAYING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, static_cast<unsigned int>(true),
								static_cast<unsigned int>(PARAM_CHANNEL::PLAY_OR_STOP_TRACK)) );
				}
				else if ( cellState == CELL_STATE::PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::NOT_PLAYING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, static_cast<unsigned int>(false),
								static_cast<unsigned int>(PARAM_CHANNEL::PLAY_OR_STOP_TRACK)) );
				}
			}
		}
		else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
		{
			if ( (m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD)
					&& (m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD) )
			{
				m_FileDeleteMode = true;
				IMnemonicParameterEventListener::PublishEvent(
						MnemonicParameterEvent(keyX, keyY, 0,
							static_cast<unsigned int>(PARAM_CHANNEL::DELETE_FILE)) );
			}
			else if ( m_Effect1BtnState == BUTTON_STATE::PRESSED || m_Effect1BtnState == BUTTON_STATE::HELD )
			{
				if ( cellState == CELL_STATE::INACTIVE )
				{
					// first ensure no recording is taking place
					bool noRecordingHappening = true;
					for ( unsigned int x = 0; x < MNEMONIC_NEOTRELLIS_COLS; x++ )
					{
						for ( unsigned int y = static_cast<unsigned int>(MNEMONIC_ROW::MIDI_CHAN_1_LOOPS);
								y < MNEMONIC_NEOTRELLIS_ROWS; y++ )
						{
							if ( m_CellStates[x][y] == CELL_STATE::RECORDING )
							{
								noRecordingHappening = false;
							}
						}
					}

					if ( noRecordingHappening )
					{
						this->setCellStateAndColor( keyX, keyY, CELL_STATE::LOADING );
						m_CachedCell.x = keyX;
						m_CachedCell.y = keyY;
						IMnemonicParameterEventListener::PublishEvent(
								MnemonicParameterEvent(keyX, keyY, 0,
									static_cast<unsigned int>(PARAM_CHANNEL::LOAD_MIDI_RECORDING)) );
					}
				}
				else if ( cellState == CELL_STATE::NOT_PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::INACTIVE );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, 0,
								static_cast<unsigned int>(PARAM_CHANNEL::UNLOAD_FILE)) );
				}
			}
			else if ( m_Effect2BtnState == BUTTON_STATE::PRESSED || m_Effect2BtnState == BUTTON_STATE::HELD )
			{
				if ( cellState == CELL_STATE::NOT_PLAYING )
				{
					// move to string edit menu
					m_StringEditModel.setString( "        " );
					m_CurrentMenu = MNEMONIC_MENUS::STRING_EDIT;
					m_CachedCell.x = keyX;
					m_CachedCell.y = keyY;
					this->draw();
				}
			}
			else
			{
				if ( cellState == CELL_STATE::INACTIVE )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::RECORDING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, 0,
								static_cast<unsigned int>(PARAM_CHANNEL::START_MIDI_RECORDING)) );
				}
				else if ( cellState == CELL_STATE::RECORDING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::NOT_PLAYING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, 0,
								static_cast<unsigned int>(PARAM_CHANNEL::END_MIDI_RECORDING)) );
				}
				else if ( cellState == CELL_STATE::NOT_PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::PLAYING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, static_cast<unsigned int>(true),
								static_cast<unsigned int>(PARAM_CHANNEL::PLAY_OR_STOP_TRACK)) );
				}
				else if ( cellState == CELL_STATE::PLAYING )
				{
					this->setCellStateAndColor( keyX, keyY, CELL_STATE::NOT_PLAYING );
					IMnemonicParameterEventListener::PublishEvent(
							MnemonicParameterEvent(keyX, keyY, static_cast<unsigned int>(false),
								static_cast<unsigned int>(PARAM_CHANNEL::PLAY_OR_STOP_TRACK)) );
				}
			}
		}
	}
	else if ( keyReleased && m_CurrentMenu == MNEMONIC_MENUS::STRING_EDIT )
	{
		const MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( m_CachedCell.y );

		// treat empty string as a cancel
		if ( strcmp(m_StringEditModel.getString(), "        ") == 0 )
		{
			m_CurrentMenu = MNEMONIC_MENUS::STATUS;
			this->draw();
			return;
		}

		if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
		{
			if ( keyX == m_CachedCell.x && keyY == m_CachedCell.y )
			{
				// send save midi file event
				IMnemonicParameterEventListener::PublishEvent(
						MnemonicParameterEvent(keyX, keyY, 0,
							static_cast<unsigned int>(PARAM_CHANNEL::SAVE_MIDI_RECORDING),
								m_StringEditModel.getString()) );
			}
		}
		else if ( row == MNEMONIC_ROW::TRANSPORT )
		{
			// send save scene file event
			IMnemonicParameterEventListener::PublishEvent(
					MnemonicParameterEvent(keyX, keyY, 0,
						static_cast<unsigned int>(PARAM_CHANNEL::SAVE_SCENE),
							m_StringEditModel.getString()) );
		}
	}
}

void MnemonicUiManager::onMnemonicUiEvent (const MnemonicUiEvent& event)
{
	UiEventType eventType = event.getEventType();

	switch ( eventType )
	{
		case UiEventType::INVALID_FILESYSTEM:
			this->displayErrorMessage( "INVALID FILESYS" );

			m_CurrentMenu = MNEMONIC_MENUS::ERROR_MESSAGE;

			break;
		case UiEventType::ENTER_STATUS_PAGE:

			break;
		case UiEventType::ENTER_FILE_EXPLORER:
		{
			// if there are no files to load, return to status
			if ( event.getDataNumElements() == 0 )
			{
				m_CurrentMenu = MNEMONIC_MENUS::STATUS;
				this->setCellStateAndColor( m_CachedCell.x, m_CachedCell.y, CELL_STATE::INACTIVE );
				return;
			}

			if ( event.getChannel() == 0 ) // entering with b12 files
			{
				m_MenuModelToUse = &m_AudioFileMenuModel;
				m_FileEntriesToUse = &m_AudioFileEntries;
			}
			else if ( event.getChannel() == 1 ) // entering with midi files
			{
				m_MenuModelToUse = &m_MidiFileMenuModel;
				m_FileEntriesToUse = &m_MidiFileEntries;
			}
			else if ( event.getChannel() == 2 ) // entering with scene files
			{
				m_MenuModelToUse = &m_SceneFileMenuModel;
				m_FileEntriesToUse = &m_SceneFileEntries;
			}


			// enter file explorer page and display list of options
			m_FileEntriesToUse->clear();
			m_MenuModelToUse->clearEntries();
			UiFileExplorerEntry* entries = reinterpret_cast<UiFileExplorerEntry*>( event.getDataPtr() );
			for ( unsigned int entryNum = 0; entryNum < event.getDataNumElements(); entryNum++ )
			{
				m_FileEntriesToUse->push_back( entries[entryNum] );
				m_MenuModelToUse->addEntry( (*m_FileEntriesToUse)[entryNum].m_FilenameDisplay );
			}

			m_MenuModelToUse->returnToTop();

			// draw the menu with the files
			this->drawScrollableMenu( *m_MenuModelToUse, nullptr, *this );

			m_CurrentMenu = MNEMONIC_MENUS::FILE_EXPLORER;
		}

			break;
		case UiEventType::TRANSPORT_MOVE:
		{
			const int currentCol = event.getChannel();
			const int previousCol = ( currentCol - 1 >= 0 ) ? currentCol - 1 : 7;
			m_Neotrellis->setColor( 0, static_cast<uint8_t>(currentCol), MNEMONIC_COLOR_TRANSPORT_ACTIVE );
			m_Neotrellis->setColor( 0, static_cast<uint8_t>(previousCol), MNEMONIC_COLOR_INACTIVE );
		}

			break;
		case UiEventType::AUDIO_TRACK_FINISHED:
			this->setCellStateAndColor( event.getCellX(), event.getCellY(), CELL_STATE::NOT_PLAYING );

			break;
		case UiEventType::MIDI_RECORDING_FINISHED:
			this->setCellStateAndColor( event.getCellX(), event.getCellY(), CELL_STATE::PLAYING );

			break;
		case UiEventType::MIDI_TRACK_FINISHED:
			this->setCellStateAndColor( event.getCellX(), event.getCellY(), CELL_STATE::NOT_PLAYING );

			break;
		case UiEventType::MIDI_TRACK_RECORDING_STATUS:
			if ( event.getChannel() == static_cast<unsigned int>(false) )
			{
				m_CurrentMenu = MNEMONIC_MENUS::MIDI_RECORDING_FAILED;
			}
			else if ( event.getChannel() == static_cast<unsigned int>(true) )
			{
				m_CurrentMenu = MNEMONIC_MENUS::STATUS;
			}

			this->draw();

			break;
		case UiEventType::MIDI_TRACK_NOT_SAVED:
		{
			std::string msgTop;
			msgTop += "MIDI TRACK AT";
			std::string msgBottom;
			msgBottom += std::to_string(event.getCellX()) + "," + std::to_string(event.getCellY()) + " UNSAVED";
			this->displayErrorMessage( msgTop, msgBottom );

			m_CurrentMenu = MNEMONIC_MENUS::STATUS;
		}

			break;
		case UiEventType::SCENE_SAVING_STATUS:
			if ( event.getChannel() == static_cast<unsigned int>(false) )
			{
				m_CurrentMenu = MNEMONIC_MENUS::SCENE_SAVING_FAILED;
			}
			else if ( event.getChannel() == static_cast<unsigned int>(true) )
			{
				m_CurrentMenu = MNEMONIC_MENUS::STATUS;
			}

			this->draw();

			break;
		case UiEventType::SCENE_TRACK_FILE_LOADED:
			this->setCellStateAndColor( event.getCellX(), event.getCellY(), CELL_STATE::NOT_PLAYING );

			break;
		default:
			break;
	}
}

void MnemonicUiManager::displayErrorMessage (const std::string& errorMessage)
{
	m_Graphics->setColor( false );
	m_Graphics->fill();

	m_Graphics->setColor( true );
	m_Graphics->drawText( 0.1f, 0.1f, errorMessage.c_str(), 1.0f );

	IMnemonicLCDRefreshEventListener::PublishEvent(
			MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
}

void MnemonicUiManager::displayErrorMessage (const std::string& errorMessageTop, const std::string& errorMessageBottom)
{
	m_Graphics->setColor( false );
	m_Graphics->fill();

	m_Graphics->setColor( true );
	m_Graphics->drawText( 0.1f, 0.1f, errorMessageTop.c_str(), 1.0f );
	m_Graphics->drawText( 0.1f, 0.25f, errorMessageBottom.c_str(), 1.0f );

	IMnemonicLCDRefreshEventListener::PublishEvent(
			MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
}

void MnemonicUiManager::drawScrollableMenu (ScrollableMenuModel& menu, bool (MnemonicUiManager::*shouldTickFunc)(unsigned int), MnemonicUiManager& ui)
{
	m_Graphics->setColor( false );
	m_Graphics->fill();

	m_Graphics->setColor( true );

	// draw cursor
	float yOffset = (0.15f * menu.getCursorIndex());
	m_Graphics->drawTriangleFilled( 0.075f, 0.1f + yOffset, 0.075f, 0.2f + yOffset, 0.125f, 0.15f + yOffset );

	// draw entries
	char** entries = menu.getEntries();
	unsigned int firstEntryIndex = menu.getFirstVisibleIndex();
	unsigned int entryNum = 0;
	char* entry = entries[entryNum];
	yOffset = 0.1f;
	const float tickOffset = 0.1f;
	while ( entry != nullptr && entryNum < MAX_ENTRIES )
	{
		const unsigned int actualIndex = firstEntryIndex + entryNum;
		if ( menu.indexIsTickable(actualIndex) )
		{
			bool fillCircle = false;
			if ( shouldTickFunc != nullptr && (ui.*shouldTickFunc)(actualIndex) )
			{
				fillCircle = true;
			}

			if ( fillCircle )
			{
				m_Graphics->drawCircleFilled( 0.175f, 0.05f + yOffset, 0.025f );
			}
			else
			{
				m_Graphics->drawCircle( 0.175f, 0.05f + yOffset, 0.025f );
			}

			m_Graphics->drawText( 0.15f + tickOffset, yOffset, entry, 1.0f );
		}
		else
		{
			m_Graphics->drawText( 0.15f, yOffset, entry, 1.0f );
		}
		yOffset += 0.15f;
		entryNum++;
		entry = entries[entryNum];
	}

	IMnemonicLCDRefreshEventListener::PublishEvent(
			MnemonicLCDRefreshEvent(0, 0, this->getFrameBuffer()->getWidth(), this->getFrameBuffer()->getHeight(), 0) );
}

void MnemonicUiManager::setCellStateAndColor (unsigned int cellX, unsigned int cellY, const CELL_STATE& state)
{
	m_CellStates[cellX][cellY] = state;

	MNEMONIC_ROW row = static_cast<MNEMONIC_ROW>( cellY );

	if ( state == CELL_STATE::INACTIVE )
	{
		m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_INACTIVE );
	}
	else if ( state == CELL_STATE::LOADING )
	{
		m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_LOADING_FILE );
	}
	else if ( state == CELL_STATE::NOT_PLAYING )
	{
		if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_LOOPS_ROW_1_NOT_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::AUDIO_LOOPS_2 )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_LOOPS_ROW_2_NOT_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::AUDIO_ONESHOTS )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_ONESHOTS_NOT_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_MIDI_NOT_PLAYING );
		}
	}
	else if ( state == CELL_STATE::PLAYING )
	{
		if ( row == MNEMONIC_ROW::AUDIO_LOOPS_1 )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_LOOPS_ROW_1_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::AUDIO_LOOPS_2 )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_LOOPS_ROW_2_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::AUDIO_ONESHOTS )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_AUDIO_ONESHOTS_PLAYING );
		}
		else if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_MIDI_PLAYING );
		}
	}
	else if ( state == CELL_STATE::RECORDING )
	{
		if ( row == MNEMONIC_ROW::MIDI_CHAN_1_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_2_LOOPS
				|| row == MNEMONIC_ROW::MIDI_CHAN_3_LOOPS || row == MNEMONIC_ROW::MIDI_CHAN_4_LOOPS )
		{
			m_Neotrellis->setColor( cellY, cellX, MNEMONIC_COLOR_MIDI_RECORDING );
		}
	}
}

CELL_STATE MnemonicUiManager::getCellState (unsigned int cellX, unsigned int cellY)
{
	return m_CellStates[cellX][cellY];
}

bool MnemonicUiManager::noCellsPlaying()
{
	for ( unsigned int row = 0; row < MNEMONIC_NEOTRELLIS_ROWS; row++ )
	{
		for ( unsigned int col = 0; col < MNEMONIC_NEOTRELLIS_COLS; col++ )
		{
			if ( this->getCellState(row, col) == CELL_STATE::PLAYING ) return false;
		}
	}

	return true;
}
