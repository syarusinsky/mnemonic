#ifndef FAKESYNTH_HPP
#define FAKESYNTH_HPP

/****************************************************************************
 * The FakeSynth class is a test class for testing MIDI looping on host. It
 * processes MIDI inputs and sets an oscillator to the appropriate frequency
 * and amplitude.
****************************************************************************/

#include "PolyBLEPOsc.hpp"
#include "IKeyEventListener.hpp"
#include <stdint.h>

class FakeSynth : public IBufferCallback<int16_t, true>, public IKeyEventListener
{
	public:
		FakeSynth (unsigned int midiChannel);
		~FakeSynth() override;

		void setOscillatorMode (const OscillatorMode& oscMode);

		void call (int16_t* writeBufferL, int16_t* writeBufferR) override;

		void onKeyEvent (const KeyEvent& keyEvent) override;

	private:
		float 		m_Amplitude;
		PolyBLEPOsc 	m_Osc;

		unsigned int 	m_MidiChannel;
};

#endif // FAKESYNTH_HPP
