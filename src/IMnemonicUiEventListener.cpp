#include "IMnemonicUiEventListener.hpp"

// instantiating IMnemonicUiEventListener's event dispatcher
EventDispatcher<IMnemonicUiEventListener, MnemonicUiEvent,
		&IMnemonicUiEventListener::onMnemonicUiEvent> IMnemonicUiEventListener::m_EventDispatcher;

MnemonicUiEvent::MnemonicUiEvent (const UiEventType& eventType, unsigned int channel) :
	IEvent( channel ),
	m_EventType( eventType )
{
}

MnemonicUiEvent::~MnemonicUiEvent()
{
}

IMnemonicUiEventListener::~IMnemonicUiEventListener()
{
	this->unbindFromMnemonicUiEventSystem();
}

void IMnemonicUiEventListener::bindToMnemonicUiEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IMnemonicUiEventListener::unbindFromMnemonicUiEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IMnemonicUiEventListener::PublishEvent (const MnemonicUiEvent& event)
{
	m_EventDispatcher.dispatch( event );
}
