#include "IMnemonicParameterEventListener.hpp"

// instantiating IMnemonicParamterEventListener's event dispatcher
EventDispatcher<IMnemonicParameterEventListener, MnemonicParameterEvent,
		&IMnemonicParameterEventListener::onMnemonicParameterEvent> IMnemonicParameterEventListener::m_EventDispatcher;

MnemonicParameterEvent::MnemonicParameterEvent (unsigned int cellX, unsigned int cellY, unsigned int value, unsigned int channel,
						const char* string) :
	IEvent( channel ),
	m_CellX( cellX ),
	m_CellY( cellY ),
	m_Value( value ),
	m_String( string )
{
}

MnemonicParameterEvent::~MnemonicParameterEvent()
{
}

unsigned int MnemonicParameterEvent::getValue() const
{
	return m_Value;
}

unsigned int MnemonicParameterEvent::getCellX() const
{
	return m_CellX;
}

unsigned int MnemonicParameterEvent::getCellY() const
{
	return m_CellY;
}

const char* MnemonicParameterEvent::getString() const
{
	return m_String;
}

IMnemonicParameterEventListener::~IMnemonicParameterEventListener()
{
	this->unbindFromMnemonicParameterEventSystem();
}

void IMnemonicParameterEventListener::bindToMnemonicParameterEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IMnemonicParameterEventListener::unbindFromMnemonicParameterEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IMnemonicParameterEventListener::PublishEvent (const MnemonicParameterEvent& paramEvent)
{
	m_EventDispatcher.dispatch( paramEvent );
}
