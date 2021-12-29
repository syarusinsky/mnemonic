#ifndef IMNEMONICUIEVENTLISTENER_HPP
#define IMNEMONICUIEVENTLISTENER_HPP

/*******************************************************************
 * An IMnemonicUiEventListener specifies a simple interface which
 * a subclass can use to be notified of mnemonic ui events. These
 * events can include error notifications.
*******************************************************************/

#include "IEventListener.hpp"

enum class UiEventType : unsigned int
{
	INVALID_FILESYSTEM // for when the sd card isn't found or the file system isn't fat16
};

class MnemonicUiEvent : public IEvent
{
	public:
		MnemonicUiEvent (const UiEventType& eventType, unsigned int channel);
		~MnemonicUiEvent() override;

		UiEventType getEventType() const { return m_EventType; }

	private:
		UiEventType m_EventType;
};

class IMnemonicUiEventListener : public IEventListener
{
	public:
		virtual ~IMnemonicUiEventListener() override;

		virtual void onMnemonicUiEvent (const MnemonicUiEvent& event) = 0;

		void bindToMnemonicUiEventSystem();
		void unbindFromMnemonicUiEventSystem();

		static void PublishEvent (const MnemonicUiEvent& event);

	private:
		static EventDispatcher<IMnemonicUiEventListener, MnemonicUiEvent,
					&IMnemonicUiEventListener::onMnemonicUiEvent> m_EventDispatcher;
};

#endif // IMNEMONICUIEVENTLISTENER_HPP
