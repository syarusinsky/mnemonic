#include "IMnemonicParameterEventListener.hpp"

// instantiating IMnemonicParamterEventListener's event dispatcher
EventDispatcher<IMnemonicParameterEventListener, MnemonicParameterEvent,
		&IMnemonicParameterEventListener::onMnemonicParameterEvent> IMnemonicParameterEventListener::m_EventDispatcher;

MnemonicParameterEvent::MnemonicParameterEvent (float value, unsigned int channel) :
	IEvent( channel ),
	m_Value( value )
{
}

MnemonicParameterEvent::~MnemonicParameterEvent()
{
}

float MnemonicParameterEvent::getValue() const
{
	return m_Value;
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
