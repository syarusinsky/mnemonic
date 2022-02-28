/*
   ==============================================================================

   This file was auto-generated!

   ==============================================================================
   */

#ifdef __unix__
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

#include "MainComponent.h"

#include "CPPFile.hpp"
#include "ColorProfile.hpp"
#include "FrameBuffer.hpp"
#include "Font.hpp"
#include "Sprite.hpp"
#include "EventQueue.hpp"
#include "IPotEventListener.hpp"

// FLUSH DENORMALS TO ZERO!
#include <xmmintrin.h>
#include <pmmintrin.h>

const unsigned int FONT_FILE_SIZE = 779;
const unsigned int LOGO_FILE_SIZE = 119;

//==============================================================================
MainComponent::MainComponent() :
	fakeNeotrellis(),
	midiHandler(),
	midiHandlerFakeSynth(),
	lastInputIndex( 0 ),
	sAudioBuffer(),
	fakeSynth(),
	fakeAxiSram{ 0 },
	sdCard( "SDCard.img" ),
	audioManager( sdCard, fakeAxiSram, sizeof(fakeAxiSram) ),
	writer(),
	effect1Sldr(),
	effect1Lbl(),
	effect2Sldr(),
	effect2Lbl(),
	effect3Sldr(),
	effect3Lbl(),
	effect1Btn( "Effect 1" ),
	effect2Btn( "Effect 2" ),
	audioSettingsBtn( "Audio Settings" ),
	midiInputList(),
	midiInputListLbl(),
	audioSettingsComponent( deviceManager, 2, 2, &audioSettingsBtn ),
	uiManager( 128, 64, CP_FORMAT::MONOCHROME_1BIT, &fakeNeotrellis ),
	screenRep( juce::Image::RGB, 256, 128, true ) // this is actually double the size so we can actually see it
{
	// FLUSH DENORMALS TO ZERO!
	_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_ON );
	_MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_ON );

	midiHandler.setShouldPublishPitch( false );
	midiHandler.setShouldPublishNoteOn( false );
	midiHandler.setShouldPublishNoteOff( false );
	midiHandlerFakeSynth.setShouldPublishMidi( false );

	// connecting to event system
	this->bindToMnemonicLCDRefreshEventSystem();
	audioManager.bindToMnemonicParameterEventSystem();
	audioManager.bindToMidiEventSystem();
	uiManager.bindToPotEventSystem();
	uiManager.bindToButtonEventSystem();
	uiManager.bindToMnemonicUiEventSystem();
	fakeSynth.bindToKeyEventSystem();

	// load font and logo from file
	char* fontBytes = new char[FONT_FILE_SIZE];
	char* logoBytes = new char[LOGO_FILE_SIZE];

	const unsigned int pathMax = 1000;
#ifdef __unix__
	char result[pathMax];
	ssize_t count = readlink( "/proc/self/exe", result, pathMax );
	std::string assetsPath( result, (count > 0) ? count : 0 );
	std::string strToRemove( "host/Builds/LinuxMakefile/build/mnemonic" );

	std::string::size_type i = assetsPath.find( strToRemove );

	if ( i != std::string::npos )
	{
		assetsPath.erase( i, strToRemove.length() );
	}

	assetsPath += "assets/";

	std::string fontPath = assetsPath + "Smoll.sff";
	std::string logoPath = assetsPath + "theroomdisconnectlogo.sif";
#elif defined(_WIN32) || defined(WIN32)
	// TODO need to actually implement this for windows
#endif
	std::ifstream fontFile( fontPath, std::ifstream::binary );
	if ( ! fontFile )
	{
		std::cout << "FAILED TO OPEN FONT FILE!" << std::endl;
	}
	fontFile.read( fontBytes, FONT_FILE_SIZE );
	fontFile.close();

	Font* font = new Font( (uint8_t*)fontBytes );
	uiManager.setFont( font );

	std::ifstream logoFile( logoPath, std::ifstream::binary );
	if ( ! logoFile )
	{
		std::cout << "FAILED TO OPEN LOGO FILE!" << std::endl;
	}
	logoFile.read( logoBytes, LOGO_FILE_SIZE );
	logoFile.close();

	Sprite* logo = new Sprite( (uint8_t*)logoBytes );

	// Some platforms require permissions to open input channels so request that here
	if ( juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
			&& ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio) )
	{
		juce::RuntimePermissions::request( juce::RuntimePermissions::recordAudio,
				[&] (bool granted) { if (granted)  setAudioChannels (2, 2); } );
	}
	else
	{
		// Specify the number of input and output channels that we want to open
		setAudioChannels (2, 2);
	}

	// connecting the audio buffer to the voice manager
	sAudioBuffer.registerCallback( &fakeSynth );
	sAudioBuffer.registerCallback( &audioManager );

	// juce audio device setup
	juce::AudioDeviceManager::AudioDeviceSetup deviceSetup = juce::AudioDeviceManager::AudioDeviceSetup();
	deviceSetup.sampleRate = 44100;
	deviceManager.initialise( 2, 2, 0, true, juce::String(), &deviceSetup );

	// basic juce logging
	// juce::Logger* log = juce::Logger::getCurrentLogger();
	// int sampleRate = deviceManager.getCurrentAudioDevice()->getCurrentSampleRate();
	// log->writeToLog( juce::String(sampleRate) );
	// log->writeToLog( juce::String(deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples()) );

	// optionally we can write wav files for debugging
	// juce::WavAudioFormat wav;
	// juce::File tempFile( "TestAudio.wav" );
	// juce::OutputStream* outStream( tempFile.createOutputStream() );
	// writer = wav.createWriterFor( outStream, sampleRate, 2, wav.getPossibleBitDepths().getLast(), NULL, 0 );

	// log->writeToLog( juce::String(wav.getPossibleBitDepths().getLast()) );
	// log->writeToLog( tempFile.getFullPathName() );

	// this file can also be used for debugging
	testFile.open( "JuceMainComponentOutput.txt" );

	// adding all child components
	addAndMakeVisible( effect1Sldr );
	effect1Sldr.setRange( 0.0f, 1.0f );
	effect1Sldr.addListener( this );
	addAndMakeVisible( effect1Lbl );
	effect1Lbl.setText( "Effect 1 Pot", juce::dontSendNotification );
	effect1Lbl.attachToComponent( &effect1Sldr, true );

	addAndMakeVisible( effect2Sldr );
	effect2Sldr.setRange( 0.0f, 1.0f );
	effect2Sldr.addListener( this );
	addAndMakeVisible( effect2Lbl );
	effect2Lbl.setText( "Effect 2 Pot", juce::dontSendNotification );
	effect2Lbl.attachToComponent( &effect2Sldr, true );

	addAndMakeVisible( effect3Sldr );
	effect3Sldr.setRange( 0.0f, 1.0f );
	effect3Sldr.addListener( this );
	addAndMakeVisible( effect3Lbl );
	effect3Lbl.setText( "Effect 3 Pot", juce::dontSendNotification );
	effect3Lbl.attachToComponent( &effect3Sldr, true );

	addAndMakeVisible( effect1Btn );
	effect1Btn.addListener( this );

	addAndMakeVisible( effect2Btn );
	effect2Btn.addListener( this );

	juce::TextButton** fakeNeotrellisBtns = fakeNeotrellis.getTextButtons();
	for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
	{
		for ( unsigned int col = 0; col < NEOTRELLIS_COLS; col++ )
		{
			addAndMakeVisible( fakeNeotrellisBtns[row][col] );
			fakeNeotrellisBtns[row][col].addListener( this );
		}
	}

	addAndMakeVisible( audioSettingsBtn );
	audioSettingsBtn.addListener( this );

	addAndMakeVisible( midiInputList );
	midiInputList.setTextWhenNoChoicesAvailable( "No MIDI Inputs Enabled" );
	auto midiInputs = juce::MidiInput::getDevices();
	midiInputList.addItemList( midiInputs, 1 );
	midiInputList.onChange = [this] { setMidiInput (midiInputList.getSelectedItemIndex()); };
	// find the first enabled device and use that by default
	for ( auto midiInput : midiInputs )
	{
		if ( deviceManager.isMidiInputEnabled (midiInput) )
		{
			setMidiInput( midiInputs.indexOf (midiInput) );
			break;
		}
	}
	// if no enabled devices were found just use the first one in the list
	if ( midiInputList.getSelectedId() == 0 )
		setMidiInput( 0 );

	addAndMakeVisible( midiInputListLbl );
	midiInputListLbl.setText( "Midi Input Device", juce::dontSendNotification );
	midiInputListLbl.attachToComponent( &midiInputList, true );

	// Make sure you set the size of the component after
	// you add any child components.
	setSize( 800, 600 );

	// verify filesystem
	audioManager.verifyFileSystem();

	// UI initialization
	uiManager.draw();

	// ARMor8PresetUpgrader presetUpgrader( initPreset, armor8VoiceManager.getPresetHeader() );
	// presetManager.upgradePresets( &presetUpgrader );

	// start timer for fake loading
	this->startTimer( 33 );

	// grab keyboard focus
	this->setWantsKeyboardFocus( true );
}

MainComponent::~MainComponent()
{
	// This shuts down the audio device and clears the audio source.
	shutdownAudio();
	delete writer;
	testFile.close();
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
	// for holding both buttons down at the same time
	if ( k.getTextCharacter() == 'z' )
	{
		effect1Btn.setState( juce::Button::ButtonState::buttonDown );
		effect2Btn.setState( juce::Button::ButtonState::buttonDown );
	}

	return true;
}

bool MainComponent::keyStateChanged (bool isKeyDown)
{
	if ( ! isKeyDown ) // if a key has been released
	{
		effect1Btn.setState( juce::Button::ButtonState::buttonNormal );
		effect2Btn.setState( juce::Button::ButtonState::buttonNormal );
	}

	return true;
}

void MainComponent::timerCallback()
{
	// these next lines are to simulate sending midi events over usart
	std::vector<MidiEvent>& midiEventsToSendVec = audioManager.getMidiEventsToSendVec();
	for ( const MidiEvent& midiEvent : midiEventsToSendVec )
	{
		const uint8_t* byteVal = midiEvent.getRawData();
		for ( unsigned int byte = 0; byte < midiEvent.getNumBytes(); byte++ )
		{
			midiHandlerFakeSynth.processByte( byteVal[byte] );
		}
	}
	midiEventsToSendVec.clear();

	midiHandlerFakeSynth.dispatchEvents();

	static unsigned int fakeLoadingCounter = 0;

	if ( fakeLoadingCounter == 100 )
	{
		fakeLoadingCounter++;

		// set preset to first preset
		// armor8VoiceManager.loadCurrentPreset();

		// uiSim.endLoading();
	}
	else if ( fakeLoadingCounter < 100 )
	{
		// uiSim.drawLoadingLogo();
		fakeLoadingCounter++;
	}
	else
	{
		// uiManager.tickForChangingBackToStatus();
		uiManager.processEffect1Btn( effect1Btn.isDown() );
		uiManager.processEffect2Btn( effect2Btn.isDown() );

		double effect1Val = effect1Sldr.getValue();
		float effect1Percentage = ( effect1Sldr.getValue() - effect1Sldr.getMinimum() )
						/ ( effect1Sldr.getMaximum() - effect1Sldr.getMinimum() );
		double effect2Val = effect2Sldr.getValue();
		float effect2Percentage = ( effect2Sldr.getValue() - effect2Sldr.getMinimum() )
						/ ( effect2Sldr.getMaximum() - effect2Sldr.getMinimum() );
		double effect3Val = effect3Sldr.getValue();
		float effect3Percentage = ( effect3Sldr.getValue() - effect3Sldr.getMinimum() )
						/ ( effect3Sldr.getMaximum() - effect3Sldr.getMinimum() );

		IPotEventListener::PublishEvent( PotEvent(effect1Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT1)) );
		IPotEventListener::PublishEvent( PotEvent(effect2Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT2)) );
		IPotEventListener::PublishEvent( PotEvent(effect3Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT3)) );
	}
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, block size, etc) are changed.

	// You can use this function to initialise any resources you might need,
	// but be careful - it will be called on the audio thread, not the GUI thread.

	// For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	// Your audio-processing code goes here!

	// For more details, see the help for AudioProcessor::getNextAudioBlock()

	// Right now we are not producing any data, in which case we need to clear the buffer
	// (to prevent the output of random noise)
	// bufferToFill.clearActiveBufferRegion();
	try
	{
		const float* inBufferL = bufferToFill.buffer->getReadPointer( 0, bufferToFill.startSample );
		const float* inBufferR = bufferToFill.buffer->getReadPointer( 1, bufferToFill.startSample );
		float* writePtrR = bufferToFill.buffer->getWritePointer( 1 );
		float* writePtrL = bufferToFill.buffer->getWritePointer( 0 );

		for ( int sample = 0; sample < bufferToFill.numSamples; sample++ )
		{
			int16_t sampleOutL = sAudioBuffer.getNextSampleL( 0 );
			int16_t sampleOutR = sAudioBuffer.getNextSampleR( 0 );
			float sampleOutFloatL = static_cast<float>( (((sampleOutL + (4096 / 2)) / 4096.0f) * 2.0f) - 1.0f );
			float sampleOutFloatR = static_cast<float>( (((sampleOutR + (4096 / 2)) / 4096.0f) * 2.0f) - 1.0f );
			writePtrR[sample] = sampleOutFloatR;
			writePtrL[sample] = sampleOutFloatL;
		}

		sAudioBuffer.pollToFillBuffers();
	}
	catch ( std::exception& e )
	{
		std::cout << "Exception caught in getNextAudioBlock: " << e.what() << std::endl;
	}
}

void MainComponent::releaseResources()
{
	// This will be called when the audio device stops, or when it is being
	// restarted due to a setting change.
	//
	// For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
	// (Our component is opaque, so we must completely fill the background with a solid colour)
	g.fillAll( getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) );

	// You can add your drawing code here!
	g.drawImageWithin( screenRep, 0, 120, getWidth(), 120, juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize );
}

void MainComponent::resized()
{
	// This is called when the MainContentComponent is resized.
	// If you add any child components, this is where you should
	// update their positions.
	int sliderLeft = 120;
	effect1Sldr.setBounds     (sliderLeft + (getWidth() / 2), 300, (getWidth() / 2) - (sliderLeft * 2), 20);
	effect2Sldr.setBounds     (sliderLeft + (getWidth() / 2), 340, (getWidth() / 2) - (sliderLeft * 2), 20);
	effect3Sldr.setBounds     (sliderLeft + (getWidth() / 2), 380, (getWidth() / 2) - (sliderLeft * 2), 20);
	effect1Btn.setBounds      (sliderLeft, 300, (getWidth() / 2) - sliderLeft - 10, 20);
	effect2Btn.setBounds      (sliderLeft, 340, (getWidth() / 2) - sliderLeft - 10, 20);
	audioSettingsBtn.setBounds(sliderLeft, 950, getWidth() - sliderLeft - 10, 20);
	midiInputList.setBounds   (sliderLeft, 980, getWidth() - sliderLeft - 10, 20);

	// grid buttons
	const unsigned int startX = sliderLeft + 10;
	const unsigned int endX = getWidth() - sliderLeft - 10;
	const unsigned int startY = 340 + 30;
	const unsigned int endY = 950 - 30;
	const unsigned int width = endX - startX;
	const unsigned int height = endY - startY;
	const unsigned int cellWidth = width / NEOTRELLIS_COLS;
	const unsigned int cellHeight = height / NEOTRELLIS_ROWS;
	juce::TextButton** fakeNeotrellisBtns = fakeNeotrellis.getTextButtons();
	for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
	{
		for ( unsigned int col = 0; col < NEOTRELLIS_COLS; col++ )
		{
			const unsigned int cellStartX = startX + ( cellWidth * col );
			const unsigned int cellStartY = startY + ( cellHeight * row );
			fakeNeotrellisBtns[row][col].setBounds( cellStartX, cellStartY, cellWidth, cellHeight );
		}
	}
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
	try
	{
	}
	catch (std::exception& e)
	{
		std::cout << "Exception caught in slider handler: " << e.what() << std::endl;
	}
}

void MainComponent::buttonStateChanged (juce::Button* button)
{
	static bool keyPressedArr[NEOTRELLIS_ROWS][NEOTRELLIS_COLS] = { false };
	try
	{
		const juce::Button::ButtonState buttonOver = juce::Button::ButtonState::buttonOver;
		const juce::Button::ButtonState buttonDown = juce::Button::ButtonState::buttonDown;
		const juce::Button::ButtonState buttonNorm = juce::Button::ButtonState::buttonNormal;

		if ( button->getState() == juce::Button::ButtonState::buttonOver || button->getState() == juce::Button::ButtonState::buttonDown )
		{
			juce::TextButton** fakeNeotrellisButtons = fakeNeotrellis.getTextButtons();
			for ( unsigned int row = 0; row < NEOTRELLIS_ROWS; row++ )
			{
				for ( unsigned int col = 0; col < NEOTRELLIS_COLS; col++ )
				{
					if ( button == &fakeNeotrellisButtons[row][col]
					  	&& (
						  	(keyPressedArr[row][col] == true && (button->getState() == buttonOver
											     || button->getState() == buttonNorm))
							|| button->getState() == buttonDown )
					   )
					{
						keyPressedArr[row][col] = button->isDown();
						fakeNeotrellis.addCellEvent( FakeNeotrellis::CellEvent{ static_cast<uint8_t>(row),
												static_cast<uint8_t>(col), ! button->isDown()} );
					}
				}
			}
		}

		fakeNeotrellis.pollForEvents();
	}
	catch (std::exception& e)
	{
		std::cout << "Exception in toggle button shit: " << e.what() << std::endl;
	}
}

void MainComponent::copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd)
{
	ColorProfile* colorProfile = uiManager.getColorProfile();
	FrameBuffer* frameBuffer = uiManager.getFrameBuffer();
	unsigned int frameBufferWidth = frameBuffer->getWidth();

	for ( unsigned int pixelY = yStart; pixelY < yEnd + 1; pixelY++ )
	{
		for ( unsigned int pixelX = xStart; pixelX < xEnd + 1; pixelX++ )
		{
			if ( ! colorProfile->getPixel(frameBuffer->getPixels(), frameBuffer->getWidth() * frameBuffer->getHeight(),
						(pixelY * frameBufferWidth) + pixelX).m_M )
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
			}
			else
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
			}
		}
	}
}

void MainComponent::setMidiInput (int index)
{
	auto list = juce::MidiInput::getDevices();

	deviceManager.removeMidiInputCallback( list[lastInputIndex], this );

	auto newInput = list[index];

	if ( !deviceManager.isMidiInputEnabled(newInput) )
		deviceManager.setMidiInputEnabled( newInput, true );

	deviceManager.addMidiInputCallback( newInput, this );
	midiInputList.setSelectedId ( index + 1, juce::dontSendNotification );

	lastInputIndex = index;
}

void MainComponent::handleIncomingMidiMessage (juce::MidiInput *source, const juce::MidiMessage &message)
{
	for ( int byte = 0; byte < message.getRawDataSize(); byte++ )
	{
		midiHandler.processByte( message.getRawData()[byte] );
	}

	midiHandler.dispatchEvents();
}

void MainComponent::onMnemonicLCDRefreshEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent)
{
	this->copyFrameBufferToImage( lcdRefreshEvent.getXStart(), lcdRefreshEvent.getYStart(),
					lcdRefreshEvent.getXEnd(), lcdRefreshEvent.getYEnd() );
	this->repaint();
}
