#ifndef IMNEMONICPARAMETEREVENTLISTENER_HPP
#define IMNEMONICPARAMETEREVENTLISTENER_HPP

/*******************************************************************
 * An IMnemonicParameterEventListener specifies a simple interface
 * which a subclass can use to be notified of Mnemonic parameter
 * change events.
*******************************************************************/

#include "IEventListener.hpp"

class MnemonicParameterEvent : public IEvent
{
	public:
		MnemonicParameterEvent (unsigned int cellX, unsigned int cellY, unsigned int value, unsigned int channel,
					const char* string = nullptr);
		~MnemonicParameterEvent() override;

		unsigned int getCellX() const;
		unsigned int getCellY() const;

		unsigned int getValue() const;

		const char* getString() const;

	private:
		unsigned int 	m_CellX;
		unsigned int 	m_CellY;
		unsigned int  	m_Value;

		const char* 	m_String;
};

class IMnemonicParameterEventListener : public IEventListener
{
	public:
		virtual ~IMnemonicParameterEventListener();

		virtual void onMnemonicParameterEvent (const MnemonicParameterEvent& paramEvent) = 0;

		void bindToMnemonicParameterEventSystem();
		void unbindFromMnemonicParameterEventSystem();

		static void PublishEvent (const MnemonicParameterEvent& paramEvent);

	private:
		static EventDispatcher<IMnemonicParameterEventListener, MnemonicParameterEvent,
					&IMnemonicParameterEventListener::onMnemonicParameterEvent> m_EventDispatcher;
};

#endif // IMNEMONICPARAMETEREVENTLISTENER_HPP
