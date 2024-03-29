#include "../../lib/STM32h745zi-HAL/llpd/include/LLPD.hpp"

#include "EventQueue.hpp"
#include "IEventListener.hpp"
#include "OLED_SH1106.hpp"
#include "MnemonicUiManager.hpp"
#include "IMnemonicLCDRefreshEventListener.hpp"
#include "IMnemonicParameterEventListener.hpp"
#include "FrameBuffer.hpp"
#include "Font.hpp"
#include "Smoll.h"

// global variables
volatile bool uiSetupComplete = false;
MnemonicUiManager* volatile uiManagerPtr = nullptr;

#define EFFECT_ADC_PORT 		GPIO_PORT::A
#define EFFECT_ADC_NUM 			ADC_NUM::ADC_1_2
#define EFFECT1_ADC_PIN 		GPIO_PIN::PIN_2
#define EFFECT1_ADC_CHANNEL 		ADC_CHANNEL::CHAN_14
#define EFFECT2_ADC_PIN 		GPIO_PIN::PIN_3
#define EFFECT2_ADC_CHANNEL 		ADC_CHANNEL::CHAN_15
#define EFFECT3_ADC_PIN 		GPIO_PIN::PIN_0
#define EFFECT3_ADC_CHANNEL 		ADC_CHANNEL::CHAN_16
#define AUDIO_IN_PORT 			GPIO_PORT::C
#define AUDIO_IN_ADC_NUM 		ADC_NUM::ADC_3
#define AUDIO1_IN_PIN 			GPIO_PIN::PIN_2
#define AUDIO1_IN_ADC_CHANNEL 		ADC_CHANNEL::CHAN_0
#define AUDIO2_IN_PIN 			GPIO_PIN::PIN_3
#define AUDIO2_IN_ADC_CHANNEL 		ADC_CHANNEL::CHAN_1
#define EFFECT_BUTTON_PORT 		GPIO_PORT::E
#define EFFECT1_BUTTON_PIN 		GPIO_PIN::PIN_0
#define EFFECT2_BUTTON_PIN 		GPIO_PIN::PIN_1
#define LOGGING_USART_NUM 		USART_NUM::USART_2
#define OLED_PORT 			GPIO_PORT::C
#define OLED_RESET_PIN 			GPIO_PIN::PIN_13
#define OLED_DC_PIN 			GPIO_PIN::PIN_14
#define OLED_CS_PIN 			GPIO_PIN::PIN_11
#define OLED_SPI_NUM 			SPI_NUM::SPI_3

// a simple class to handle lcd refresh events
class Oled_Manager : public IMnemonicLCDRefreshEventListener
{
	public:
		Oled_Manager (uint8_t* displayBuffer) :
			m_Oled( OLED_SPI_NUM, OLED_PORT, OLED_CS_PIN, OLED_PORT, OLED_DC_PIN, OLED_PORT, OLED_RESET_PIN ),
			m_DisplayBuffer( displayBuffer )
		{
			LLPD::gpio_output_setup( OLED_PORT, OLED_CS_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_PORT, OLED_CS_PIN, true );
			LLPD::gpio_output_setup( OLED_PORT, OLED_DC_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_PORT, OLED_DC_PIN, true );
			LLPD::gpio_output_setup( OLED_PORT, OLED_RESET_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_PORT, OLED_RESET_PIN, true );

			m_Oled.begin();

			this->bindToMnemonicLCDRefreshEventSystem();
		}
		~Oled_Manager() override
		{
			this->unbindFromMnemonicLCDRefreshEventSystem();
		}

		void onMnemonicLCDRefreshEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent) override
		{
			unsigned int columnStart = lcdRefreshEvent.getXStart();
			unsigned int rowStart = lcdRefreshEvent.getYStart();
			unsigned int columnEnd = lcdRefreshEvent.getXEnd();
			unsigned int rowEnd = lcdRefreshEvent.getYEnd();
			if ( columnEnd - columnStart == SH1106_LCDWIDTH && rowEnd - rowStart == SH1106_LCDHEIGHT )
			{
				m_Oled.displayFullRowMajor( m_DisplayBuffer );
			}
			else
			{
				// offset since display is flipped
				rowStart = SH1106_LCDHEIGHT - 1 - rowStart;
				rowEnd = SH1106_LCDHEIGHT - 1 - rowEnd;
				columnStart = SH1106_LCDWIDTH - 1 - columnStart;
				columnEnd = SH1106_LCDWIDTH - 1 - columnEnd;

				// swap since display is flipped
				unsigned int temp = rowStart;
				rowStart = rowEnd;
				rowEnd = temp;
				temp = columnStart;
				columnStart = columnEnd;
				columnEnd = temp;

				m_Oled.displayPartialRowMajor( m_DisplayBuffer, rowStart, columnStart, rowEnd, columnEnd );
			}
		}

	private:
		Oled_SH1106 	m_Oled;
		uint8_t* 	m_DisplayBuffer;
};

// this class is to receive the published parameter events from ui manager and put them in the event queue for the m7 core to retrieve
class MnemonicParameterEventBridge : public IMnemonicParameterEventListener
{
	public:
		MnemonicParameterEventBridge (EventQueue<MnemonicParameterEvent>* eventQueuePtr) : m_EventQueuePtr( eventQueuePtr ) {}
		~MnemonicParameterEventBridge() override {}

		void onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent) override
		{
			m_EventQueuePtr->writeEvent( paramEvent );
		}

	private:
		EventQueue<MnemonicParameterEvent>* m_EventQueuePtr;
};

// this class is to retrieve preset events from the m7 core via the preset event queue and republish them to the ui manager
class MnemonicUiEventBridge
{
	public:
		MnemonicUiEventBridge (EventQueue<MnemonicUiEvent>* eventQueuePtr) : m_EventQueuePtr( eventQueuePtr ) {}
		~MnemonicUiEventBridge() {}

		void processQueuedUiEvents()
		{
			MnemonicUiEvent uiEvent( UiEventType::INVALID_FILESYSTEM, nullptr, 0, 0 );
			bool readCorrectly = m_EventQueuePtr->readEvent( uiEvent );
			while ( readCorrectly )
			{
				IMnemonicUiEventListener::PublishEvent( uiEvent );

				readCorrectly = m_EventQueuePtr->readEvent( uiEvent );
			}
		}

	private:
		EventQueue<MnemonicUiEvent>* m_EventQueuePtr;
};

int main(void)
{
	LLPD::rcc_clock_start_max_cpu2();

	EventQueue<MnemonicParameterEvent>* paramEventQueue = nullptr;
	EventQueue<MnemonicUiEvent>* uiEventQueue = nullptr;

	// wait for setupCompleteFlag to inform that audio timer is setup
	while ( true )
	{
		if ( LLPD::hsem_try_take(0) )
		{
			bool* volatile setupCompleteFlag = reinterpret_cast<bool*>( D2_AHBSRAM_BASE );
			if ( *setupCompleteFlag == true )
			{
				uint8_t* sram4Ptr = reinterpret_cast<uint8_t*>( D3_SRAM_BASE ) + ( D3_SRAM_UNUSED_OFFSET_IN_BYTES );

				paramEventQueue = new ( sram4Ptr ) EventQueue<MnemonicParameterEvent>(
									sram4Ptr + sizeof(EventQueue<MnemonicParameterEvent>),
									sizeof(MnemonicParameterEvent) * MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE, 1 );
				uint8_t* uiEventQueueMem = sram4Ptr + sizeof( EventQueue<MnemonicParameterEvent> )
									+ ( sizeof(MnemonicParameterEvent) * MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE );
				uiEventQueue = reinterpret_cast<EventQueue<MnemonicUiEvent>*>( uiEventQueueMem );

				*setupCompleteFlag = false;

				LLPD::hsem_release( 0 );

				break;
			}

			LLPD::hsem_release( 0 );
		}
	}

	// UI manager setup
	LLPD::gpio_digital_input_setup( GPIO_PORT::A, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_UP );
	uint8_t i2cAddresses[] = { 0x2F, 0x2E, 0x31, 0x30 };
	Multitrellis multitrellis( 2, 2, I2C_NUM::I2C_2, i2cAddresses, GPIO_PORT::A, GPIO_PIN::PIN_8 );
	Font font( Smoll_data );
	MnemonicUiManager uiManager( SH1106_LCDWIDTH, SH1106_LCDHEIGHT, CP_FORMAT::MONOCHROME_1BIT, &multitrellis );
	uiManager.bindToButtonEventSystem();
	uiManager.bindToPotEventSystem();
	uiManager.bindToMnemonicUiEventSystem();
	uiManager.setFont( &font );
	uiManagerPtr = &uiManager;

	// parameter event bridge setup
	MnemonicParameterEventBridge paramEventBridge( paramEventQueue );
	paramEventBridge.bindToMnemonicParameterEventSystem();

	// preset event bridge setup
	MnemonicUiEventBridge uiEventBridge( uiEventQueue );

	// OLED setup
	Oled_Manager oled( uiManager.getFrameBuffer()->getPixels() );

	uiSetupComplete = true;

	while ( true )
	{
		multitrellis.pollForEvents();

		uiEventBridge.processQueuedUiEvents();

		uiManager.processEffect1Btn( ! LLPD::gpio_input_get(EFFECT_BUTTON_PORT, EFFECT1_BUTTON_PIN) );
		uiManager.processEffect2Btn( ! LLPD::gpio_input_get(EFFECT_BUTTON_PORT, EFFECT2_BUTTON_PIN) );

		uint16_t effect1Val = LLPD::adc_get_channel_value( EFFECT_ADC_NUM, EFFECT1_ADC_CHANNEL );
		float effect1Percentage = static_cast<float>( effect1Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(effect1Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT1)) );

		uint16_t effect2Val = LLPD::adc_get_channel_value( EFFECT_ADC_NUM, EFFECT2_ADC_CHANNEL );
		float effect2Percentage = static_cast<float>( effect2Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(effect2Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT2)) );

		uint16_t effect3Val = LLPD::adc_get_channel_value( EFFECT_ADC_NUM, EFFECT3_ADC_CHANNEL );
		float effect3Percentage = static_cast<float>( effect3Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(effect3Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT3)) );
	}
}
