/*
   ==============================================================================

   This file was auto-generated!

   ==============================================================================
   */

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "AudioBuffer.hpp"
#include "MidiHandler.hpp"
#include "PresetManager.hpp"
#include "AudioSettingsComponent.h"
#include "MnemonicConstants.hpp"
#include "MnemonicAudioManager.hpp"
#include "MnemonicUiManager.hpp"
#include "IMnemonicLCDRefreshEventListener.hpp"
#include "CPPFile.hpp"
#include "FakeSynth.hpp"
#include "Neotrellis.hpp"

#include <iostream>
#include <fstream>

constexpr unsigned int NEOTRELLIS_ROWS = 8;
constexpr unsigned int NEOTRELLIS_COLS = 8;

// class to emulate neotrellis
class FakeNeotrellis : public NeotrellisInterface
{
	public:
		struct CellEvent
		{
			uint8_t keyX;
			uint8_t keyY;
			bool    keyReleased;
		};

		FakeNeotrellis() : m_FakeButtons( new juce::TextButton*[NEOTRELLIS_ROWS] )
		{
			for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
			{
				m_FakeButtons[row] = new juce::TextButton[NEOTRELLIS_COLS];

				for ( unsigned int col = 0; col < NEOTRELLIS_COLS; col++ )
				{
					m_Callbacks[col + (row * NEOTRELLIS_COLS)] = nullptr;
				}
			}
		}
		~FakeNeotrellis() override
		{
			for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
			{
				delete[] m_FakeButtons[row];
			}

			delete[] m_FakeButtons;
		}

		juce::TextButton** getTextButtons() { return m_FakeButtons; }

		void addCellEvent (const CellEvent& cellEvent)
		{
			m_CellEvents.push_back( cellEvent );
		}

		void begin (NeotrellisListener* listener) override
		{
			NeotrellisInterface::begin( listener );

			for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
			{
				for ( unsigned int col = 0; col < NEOTRELLIS_COLS; col++ )
				{
					this->setColor( row, col, 255, 255, 255 );
				}
			}
		}

		void setColor (uint8_t keyRow, uint8_t keyCol, uint8_t r, uint8_t g, uint8_t b) override
		{
			m_FakeButtons[keyRow][keyCol].setColour( juce::TextButton::buttonColourId, juce::Colour(r, g, b) );
		}

		void pollForEvents() override
		{
			for ( const CellEvent& cellEvent : m_CellEvents )
			{
				const uint8_t index = cellEvent.keyX + ( cellEvent.keyY * NEOTRELLIS_COLS );

				if ( m_Callbacks[index] != nullptr )
				{
					m_Callbacks[index]( m_NeotrellisListener, this, cellEvent.keyReleased, cellEvent.keyX, cellEvent.keyY );
				}
			}

			m_CellEvents.clear();
		}

		void registerCallback (uint8_t keyRow, uint8_t keyCol, NeotrellisCallback callback) override
		{
			const uint8_t index = keyCol + ( keyRow * NEOTRELLIS_COLS );
			m_Callbacks[index] = callback;
		}

		uint8_t getNumRows() override { return NEOTRELLIS_ROWS; }
		uint8_t getNumCols() override { return NEOTRELLIS_COLS; }

	private:
		std::vector<CellEvent> 	m_CellEvents;
		juce::TextButton** 	m_FakeButtons;

		NeotrellisCallback 	m_Callbacks[NEOTRELLIS_ROWS * NEOTRELLIS_COLS];
};

//==============================================================================
/*
   This component lives inside our window, and this is where you should put all
   your controls and content.
   */
class MainComponent   : public juce::AudioAppComponent, public juce::Slider::Listener, public juce::Button::Listener,
			public juce::MidiInputCallback, public juce::Timer, public IMnemonicLCDRefreshEventListener
{
	public:
		//==============================================================================
		MainComponent();
		~MainComponent();

		//==============================================================================
		void timerCallback() override;

		//==============================================================================
		void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
		void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
		void releaseResources() override;

		//==============================================================================
		void paint (juce::Graphics& g) override;
		void resized() override;

		//==============================================================================
		bool keyPressed (const juce::KeyPress& k) override;
		bool keyStateChanged (bool isKeyDown) override;

		void sliderValueChanged (juce::Slider* slider) override;
		void buttonClicked (juce::Button* button) override {}
		void buttonStateChanged (juce::Button* button) override;
		void updateToggleState (juce::Button* button);

		void setMidiInput (int index);
		void handleIncomingMidiMessage (juce::MidiInput *source, const juce::MidiMessage &message) override;

		void onMnemonicLCDRefreshEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent) override;

	private:
		//==============================================================================
		// Your private member variables go here...
		// PresetManager presetManager;
		FakeNeotrellis fakeNeotrellis;
		MidiHandler midiHandler;
		MidiHandler midiHandlerFakeSynth;
		int lastInputIndex;
		::AudioBuffer<int16_t, true> sAudioBuffer;
		FakeSynth fakeSynth;

		uint8_t fakeAxiSram[524288]; // 512kB

		CPPFile sdCard;

		MnemonicAudioManager audioManager;

		juce::AudioFormatWriter* writer;

		juce::Slider effect1Sldr;
		juce::Label effect1Lbl;
		juce::Slider effect2Sldr;
		juce::Label effect2Lbl;
		juce::Slider effect3Sldr;
		juce::Label effect3Lbl;

		juce::TextButton effect1Btn;
		juce::TextButton effect2Btn;

		juce::TextButton audioSettingsBtn;

		juce::ComboBox midiInputList;
		juce::Label midiInputListLbl;

		AudioSettingsComponent audioSettingsComponent;

		MnemonicUiManager uiManager;

		juce::Image screenRep;

		std::ofstream testFile;

		void copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd);

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
