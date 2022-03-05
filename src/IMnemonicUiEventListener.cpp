#include "IMnemonicUiEventListener.hpp"

// instantiating IMnemonicUiEventListener's event dispatcher
EventDispatcher<IMnemonicUiEventListener, MnemonicUiEvent,
		&IMnemonicUiEventListener::onMnemonicUiEvent> IMnemonicUiEventListener::m_EventDispatcher;

MnemonicUiEvent::MnemonicUiEvent (const UiEventType& eventType, void* dataPtr, unsigned int dataNumElements, unsigned int channel,
					unsigned int cellX, unsigned int cellY) :
	IEvent( channel ),
	m_EventType( eventType ),
	m_DataPtr( dataPtr ),
	m_DataNumElements( dataNumElements ),
	m_CellX( cellX ),
	m_CellY( cellY )
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
