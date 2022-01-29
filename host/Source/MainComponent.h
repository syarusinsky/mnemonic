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

#include <iostream>
#include <fstream>

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
		void buttonClicked (juce::Button* button) override;
		void updateToggleState (juce::Button* button);

		void setMidiInput (int index);
		void handleIncomingMidiMessage (juce::MidiInput *source, const juce::MidiMessage &message) override;

		void onMnemonicLCDRefreshEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent) override;

	private:
		//==============================================================================
		// Your private member variables go here...
		// PresetManager presetManager;
		MidiHandler midiHandler;
		int lastInputIndex;
		::AudioBuffer<int16_t, true> sAudioBuffer;

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
