#include "../../lib/STM32h745zi-HAL/llpd/include/LLPD.hpp"

#include "EEPROM_CAT24C64.hpp"
#include "SRAM_23K256.hpp"
#include "SDCard.hpp"
#include "EventQueue.hpp"
#include "MidiHandler.hpp"
#include "MnemonicAudioManager.hpp"
#include "AudioBuffer.hpp"
#include "AudioConstants.hpp"
#include "IMnemonicUiEventListener.hpp"

const int SYS_CLOCK_FREQUENCY = 480000000;

// global variables
MidiHandler* volatile midiHandlerPtr = nullptr;
AudioBuffer<int16_t, true>* volatile audioBufferPtr = nullptr;

// peripheral defines
#define OP_AMP1_INV_OUT_PORT 		GPIO_PORT::C
#define OP_AMP1_NONINVR_PORT 		GPIO_PORT::B
#define OP_AMP1_INVERT_PIN 		GPIO_PIN::PIN_5
#define OP_AMP1_OUTPUT_PIN 		GPIO_PIN::PIN_4
#define OP_AMP1_NON_INVERT_PIN 		GPIO_PIN::PIN_0
#define OP_AMP2_PORT 			GPIO_PORT::E
#define OP_AMP2_INVERT_PIN 		GPIO_PIN::PIN_8
#define OP_AMP2_OUTPUT_PIN 		GPIO_PIN::PIN_7
#define OP_AMP2_NON_INVERT_PIN 		GPIO_PIN::PIN_9
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
#define SRAM_CS_PORT 			GPIO_PORT::D
#define SRAM1_CS_PIN 			GPIO_PIN::PIN_8
#define SRAM2_CS_PIN 			GPIO_PIN::PIN_9
#define SRAM3_CS_PIN 			GPIO_PIN::PIN_10
#define SRAM4_CS_PIN 			GPIO_PIN::PIN_11
#define EEPROM1_ADDRESS 		false, false, false
#define EEPROM2_ADDRESS 		true, false, false
#define EEPROM3_ADDRESS 		false, true, false
#define EEPROM4_ADDRESS 		true, true, false
#define SD_CARD_CS_PORT 		GPIO_PORT::E
#define SD_CARD_CS_PIN 			GPIO_PIN::PIN_15
#define OLED_PORT 			GPIO_PORT::C
#define OLED_RESET_PIN 			GPIO_PIN::PIN_13
#define OLED_DC_PIN 			GPIO_PIN::PIN_14
#define OLED_CS_PIN 			GPIO_PIN::PIN_11
#define MIDI_USART_NUM 			USART_NUM::USART_6
#define LOGGING_USART_NUM 		USART_NUM::USART_2
#define EEPROM_I2C_NUM 			I2C_NUM::I2C_1
#define SRAM_SPI_NUM 			SPI_NUM::SPI_2
#define SD_CARD_SPI_NUM 		SPI_NUM::SPI_4
#define OLED_SPI_NUM 			SPI_NUM::SPI_3

// this class is specifically to check if the eeprom has been initialized with the correct code at the end of the eeprom addresses
class Eeprom_CAT24C64_Manager_ARMor8 : public Eeprom_CAT24C64_Manager
{
	public:
		Eeprom_CAT24C64_Manager_ARMor8 (const I2C_NUM& i2cNum, const std::vector<Eeprom_CAT24C64_AddressConfig>& addressConfigs) :
			Eeprom_CAT24C64_Manager( i2cNum, addressConfigs ) {}

		bool needsInitialization() override
		{
			for ( unsigned int byte = 0; byte < sizeof(m_InitCode); byte++ )
			{
				uint8_t value = this->readByte( m_InitCodeStartAddress + byte );
				if ( value != m_InitCode[byte] )
				{
					// LLPD::usart_log( LOGGING_USART_NUM, "EEPROM init code not detected, initializing now..." );

					return true;
				}
			}

			// LLPD::usart_log( LOGGING_USART_NUM, "EEPROM init code detected, loading preset now..." );

			return false;
		}

		void afterInitialize() override
		{
			for ( unsigned int byte = 0; byte < sizeof(m_InitCode); byte++ )
			{
				this->writeByte( m_InitCodeStartAddress + byte, m_InitCode[byte] );
			}

			for ( unsigned int byte = 0; byte < sizeof(m_InitCode); byte++ )
			{
				uint8_t value = this->readByte( m_InitCodeStartAddress + byte );
				if ( value != m_InitCode[byte] )
				{
					// LLPD::usart_log( LOGGING_USART_NUM, "EEPROM failed to initialize, check connections and setup..." );

					return;
				}
			}

			// LLPD::usart_log( LOGGING_USART_NUM, "EEPROM initialized successfully, loading preset now..." );
		}

	private:
		uint8_t m_InitCode[8] = { 0b01100100, 0b11111110, 0b10000010, 0b10110110, 0b01001110, 0b11100011, 0b00110110, 0b10101111 };
		unsigned int m_InitCodeStartAddress = ( Eeprom_CAT24C64::EEPROM_SIZE * m_Eeproms.size() ) - sizeof( m_InitCode );
};

// this class is to retrieve parameter events from the m4 core via the parameter event queue and republish to the voice manager
class MnemonicParameterEventBridge
{
	public:
		MnemonicParameterEventBridge (EventQueue<MnemonicParameterEvent>* eventQueuePtr) : m_EventQueuePtr( eventQueuePtr ) {}
		~MnemonicParameterEventBridge() {}

		void processQueuedParameterEvents()
		{
			MnemonicParameterEvent paramEvent( 0, 0, 0, 0 );
			bool readCorrectly = m_EventQueuePtr->readEvent( paramEvent );
			while ( readCorrectly )
			{
				IMnemonicParameterEventListener::PublishEvent( paramEvent );

				readCorrectly = m_EventQueuePtr->readEvent( paramEvent );
			}
		}

	private:
		EventQueue<MnemonicParameterEvent>* m_EventQueuePtr;
};

// this class is to receive the published ui events from the voice manager and put them in the event queue for the m4 core to retrieve
class MnemonicUiEventBridge : public IMnemonicUiEventListener
{
	public:
		MnemonicUiEventBridge (EventQueue<MnemonicUiEvent>* eventQueuePtr) : m_EventQueuePtr( eventQueuePtr ) {}
		~MnemonicUiEventBridge() override {}

		void onMnemonicUiEvent (const MnemonicUiEvent& uiEvent) override
		{
			m_EventQueuePtr->writeEvent( uiEvent );
		}

	private:
		EventQueue<MnemonicUiEvent>* m_EventQueuePtr;
};

// these pins are unused for mnemonic, so we disable them as per the ST recommendations
void disableUnusedPins()
{
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_1, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_7, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_12, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_0, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_1, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_2, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_4, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_12, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_13, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_14, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_0, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_1, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_2, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_3, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_4, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_5, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_0, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_1, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_2, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_3, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_4, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_5, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_7, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_12, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_13, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_14, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::D, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_2, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_3, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_4, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_5, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_7, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::E, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_7, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_14, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::F, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_7, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_9, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_10, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_11, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_12, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_13, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::G, GPIO_PIN::PIN_14, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( AUDIO_IN_PORT, AUDIO1_IN_PIN, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( AUDIO_IN_PORT, AUDIO2_IN_PIN, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
}

int main(void)
{
	// setup clock 480MHz (also prescales peripheral clocks to fit rate limitations)
	LLPD::rcc_clock_start_max_cpu1();
	LLPD::rcc_start_pll2();

	// enable gpio clocks
	LLPD::gpio_enable_clock( GPIO_PORT::A );
	LLPD::gpio_enable_clock( GPIO_PORT::B );
	LLPD::gpio_enable_clock( GPIO_PORT::C );
	LLPD::gpio_enable_clock( GPIO_PORT::D );
	LLPD::gpio_enable_clock( GPIO_PORT::E );
	LLPD::gpio_enable_clock( GPIO_PORT::F );
	LLPD::gpio_enable_clock( GPIO_PORT::G );
	LLPD::gpio_enable_clock( GPIO_PORT::H );

	// disable unused pins
	// TODO renable this when working
	// disableUnusedPins();

	// USART setup
	// LLPD::usart_init( LOGGING_USART_NUM, USART_WORD_LENGTH::BITS_8, USART_PARITY::NONE, USART_CONF::TX_AND_RX,
	// 			USART_STOP_BITS::BITS_1, 120000000, 9600 );
	// LLPD::usart_log( LOGGING_USART_NUM, "Ultra_FX_SYN starting up ----------------------------" );
	LLPD::usart_init( MIDI_USART_NUM, USART_WORD_LENGTH::BITS_8, USART_PARITY::NONE, USART_CONF::TX_AND_RX,
					USART_STOP_BITS::BITS_1, 120000000, 31250 );

	// audio timer setup (for 40 kHz sampling rate at 480 MHz timer clock)
	LLPD::tim6_counter_setup( 0, 12000, 40000 );
	LLPD::tim6_counter_enable_interrupts();
	// LLPD::usart_log( LOGGING_USART_NUM, "tim6 initialized..." );

	// DAC setup
	LLPD::dac_init( true );
	// LLPD::usart_log( LOGGING_USART_NUM, "dac initialized..." );

	// Op Amp setup
	LLPD::gpio_analog_setup( OP_AMP1_INV_OUT_PORT, OP_AMP1_INVERT_PIN );
	LLPD::gpio_analog_setup( OP_AMP1_INV_OUT_PORT, OP_AMP1_OUTPUT_PIN );
	LLPD::gpio_analog_setup( OP_AMP1_NONINVR_PORT, OP_AMP1_NON_INVERT_PIN );
	LLPD::opamp_init( OPAMP_NUM::OPAMP_1 );
	LLPD::gpio_analog_setup( OP_AMP2_PORT, OP_AMP2_INVERT_PIN );
	LLPD::gpio_analog_setup( OP_AMP2_PORT, OP_AMP2_OUTPUT_PIN );
	LLPD::gpio_analog_setup( OP_AMP2_PORT, OP_AMP2_NON_INVERT_PIN );
	LLPD::opamp_init( OPAMP_NUM::OPAMP_2 );
	LLPD::usart_log( LOGGING_USART_NUM, "op amp initialized..." );

	// spi initialization
	LLPD::spi_master_init( OLED_SPI_NUM, SPI_BAUD_RATE::SYSCLK_DIV_BY_32, SPI_CLK_POL::LOW_IDLE, SPI_CLK_PHASE::FIRST,
				SPI_DUPLEX::FULL, SPI_FRAME_FORMAT::MSB_FIRST, SPI_DATA_SIZE::BITS_8 );
	LLPD::spi_master_init( SD_CARD_SPI_NUM, SPI_BAUD_RATE::SYSCLK_DIV_BY_32, SPI_CLK_POL::LOW_IDLE, SPI_CLK_PHASE::FIRST,
				SPI_DUPLEX::FULL, SPI_FRAME_FORMAT::MSB_FIRST, SPI_DATA_SIZE::BITS_8 );
	// LLPD::usart_log( LOGGING_USART_NUM, "spi initialized..." );

	// i2c initialization
	LLPD::i2c_master_setup( EEPROM_I2C_NUM, 0x308075AE );
	LLPD::i2c_master_setup( I2C_NUM::I2C_2, 0x308075AE );
	// LLPD::usart_log( LOGGING_USART_NUM, "i2c initialized..." );

	// audio timer start
	LLPD::tim6_counter_start();
	// LLPD::usart_log( LOGGING_USART_NUM, "tim6 started..." );

	// adc setup (note this must be done after the tim6_counter_start() call since it uses the delay funtion)
	LLPD::gpio_analog_setup( EFFECT_ADC_PORT, EFFECT1_ADC_PIN );
	LLPD::gpio_analog_setup( EFFECT_ADC_PORT, EFFECT2_ADC_PIN );
	LLPD::gpio_analog_setup( EFFECT_ADC_PORT, EFFECT3_ADC_PIN );
	LLPD::adc_init( ADC_NUM::ADC_1_2, ADC_CYCLES_PER_SAMPLE::CPS_64p5 );
	LLPD::adc_set_channel_order( ADC_NUM::ADC_1_2, 3, EFFECT1_ADC_CHANNEL, EFFECT2_ADC_CHANNEL, EFFECT3_ADC_CHANNEL );

	// pushbutton setup
	LLPD::gpio_digital_input_setup( EFFECT_BUTTON_PORT, EFFECT1_BUTTON_PIN, GPIO_PUPD::PULL_UP );
	LLPD::gpio_digital_input_setup( EFFECT_BUTTON_PORT, EFFECT2_BUTTON_PIN, GPIO_PUPD::PULL_UP );

	// EEPROM setup and test
	std::vector<Eeprom_CAT24C64_AddressConfig> eepromAddressConfigs;
	eepromAddressConfigs.emplace_back( EEPROM1_ADDRESS );
	eepromAddressConfigs.emplace_back( EEPROM2_ADDRESS );
	eepromAddressConfigs.emplace_back( EEPROM3_ADDRESS );
	eepromAddressConfigs.emplace_back( EEPROM4_ADDRESS );
	Eeprom_CAT24C64_Manager_ARMor8 eeproms( EEPROM_I2C_NUM, eepromAddressConfigs );

	// SD Card setup
	LLPD::gpio_output_setup( SD_CARD_CS_PORT, SD_CARD_CS_PIN, GPIO_PUPD::PULL_UP, GPIO_OUTPUT_TYPE::OPEN_DRAIN, GPIO_OUTPUT_SPEED::VERY_HIGH, false );
	LLPD::gpio_output_set( SD_CARD_CS_PORT, SD_CARD_CS_PIN, true );
	SDCard sdCard( SD_CARD_SPI_NUM, SD_CARD_CS_PORT, SD_CARD_CS_PIN );
	sdCard.initialize();
	// boost SPI clock after initialization
	LLPD::spi_master_change_baud_rate( SD_CARD_SPI_NUM, SPI_BAUD_RATE::SYSCLK_DIV_BY_2 );
	// LLPD::usart_log( LOGGING_USART_NUM, "sd card initialized..." );

	// LLPD::usart_log( LOGGING_USART_NUM, "Ultra_FX_SYN setup complete, entering while loop -------------------------------" );

	// setup midi handler
	MidiHandler midiHandler;
	midiHandlerPtr = &midiHandler;

	// setup preset event queue (memory comes after parameter event queue)
	uint8_t* uiEventQueueMem = reinterpret_cast<uint8_t*>( D3_SRAM_BASE ) + ( D3_SRAM_UNUSED_OFFSET_IN_BYTES )
									+ sizeof( EventQueue<MnemonicParameterEvent> )
									+ ( sizeof(MnemonicParameterEvent) * MNEMONIC_PARAMETER_EVENT_QUEUE_SIZE );
	EventQueue<MnemonicUiEvent>* uiEventQueue = new ( uiEventQueueMem ) EventQueue<MnemonicUiEvent>( uiEventQueueMem
								+ sizeof(EventQueue<MnemonicUiEvent>), sizeof(MnemonicUiEvent)
								* MNEMONIC_UI_EVENT_QUEUE_SIZE, 2 );
	MnemonicUiEventBridge uiEventBridge( uiEventQueue );
	uiEventBridge.bindToMnemonicUiEventSystem();

	// inform CM4 core that setup is complete
	while ( ! LLPD::hsem_try_take(0) ) {}
	bool* volatile setupCompleteFlag = reinterpret_cast<bool*>( D2_AHBSRAM_BASE );
	*setupCompleteFlag = true;
	LLPD::hsem_release( 0 );

	// wait for CM4 core setup complete
	while ( true )
	{
		if ( LLPD::hsem_try_take(0) )
		{
			if ( *setupCompleteFlag == false )
			{
				LLPD::hsem_release( 0 );

				break;
			}

			LLPD::hsem_release( 0 );
		}
	}

	// flush denormals
	__set_FPSCR( __get_FPSCR() | (1 << 24) );

	// prepare event queues
	uint8_t* paramEventQueueMem = reinterpret_cast<uint8_t*>( D3_SRAM_BASE ) + ( D3_SRAM_UNUSED_OFFSET_IN_BYTES );
	EventQueue<MnemonicParameterEvent>* paramEventQueue = reinterpret_cast<EventQueue<MnemonicParameterEvent>*>( paramEventQueueMem );
	MnemonicParameterEventBridge paramEventBridge( paramEventQueue );

	// prepare audio manager
	MnemonicAudioManager audioManager( sdCard, reinterpret_cast<uint8_t*>(D1_AXISRAM_BASE), 524288 );
	audioManager.bindToMnemonicParameterEventSystem();
	audioManager.bindToMidiEventSystem();

	// connect to audio buffer
	AudioBuffer<int16_t, true> audioBuffer;
	audioBuffer.registerCallback( &audioManager );
	audioBufferPtr = &audioBuffer;

	// verify fat16 file system
	audioManager.verifyFileSystem();

	// enable instruction cache
	SCB_EnableICache();

	while ( true )
	{
		LLPD::adc_perform_conversion_sequence( EFFECT_ADC_NUM );

		paramEventBridge.processQueuedParameterEvents();

		midiHandler.dispatchEvents();

		audioBuffer.pollToFillBuffers();

		for ( const MidiEvent& midiEvent : audioManager.getMidiEventsToSendVec() )
		{
			uint8_t* midiRawData = midiEvent.getRawData();

			if ( midiEvent.getNumBytes() > 1 )
			{
				for ( unsigned int byteNum = 0; byteNum < midiEvent.getNumBytes(); byteNum++ )
				{
					LLPD::usart_transmit( MIDI_USART_NUM, midiRawData[byteNum] );
				}
			}
		}
		audioManager.getMidiEventsToSendVec().clear();

		audioManager.publishUiEvents();
	}
}

extern "C" void TIM6_DAC_IRQHandler (void)
{
	if ( ! LLPD::tim6_isr_handle_delay() ) // if not currently in a delay function,...
	{
		if ( audioBufferPtr )
		{
			uint16_t outValL = audioBufferPtr->getNextSampleL( 0 ) + ( 4096 / 2 );
			uint16_t outValR = audioBufferPtr->getNextSampleR( 0 ) + ( 4096 / 2 );

			LLPD::dac_send( outValL, outValR );
		}
	}

	LLPD::tim6_counter_clear_interrupt_flag();
}

extern "C" void USART2_IRQHandler (void) // logging usart
{
	/*
	// loopback test code for usart recieve
	uint16_t data = LLPD::usart_receive( LOGGING_USART_NUM );
	LLPD::usart_transmit( LOGGING_USART_NUM, data );
	*/
}

extern "C" void USART6_IRQHandler (void) // midi usart
{
	uint16_t data = LLPD::usart_receive( MIDI_USART_NUM );
	if ( midiHandlerPtr )
	{
		midiHandlerPtr->processByte( data );
	}
}
