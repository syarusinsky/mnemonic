#include "IMnemonicLCDRefreshEventListener.hpp"

// instantiating IMnemonicLCDRefreshEventListener's event dispatcher
EventDispatcher<IMnemonicLCDRefreshEventListener, MnemonicLCDRefreshEvent,
		&IMnemonicLCDRefreshEventListener::onMnemonicLCDRefreshEvent> IMnemonicLCDRefreshEventListener::m_EventDispatcher;

MnemonicLCDRefreshEvent::MnemonicLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
						unsigned int channel) :
	IEvent( channel ),
	m_XStart( xStart ),
	m_YStart( yStart ),
	m_XEnd( xEnd ),
	m_YEnd( yEnd )
{
}

MnemonicLCDRefreshEvent::~MnemonicLCDRefreshEvent()
{
}

unsigned int MnemonicLCDRefreshEvent::getXStart() const
{
	return m_XStart;
}

unsigned int MnemonicLCDRefreshEvent::getYStart() const
{
	return m_YStart;
}

unsigned int MnemonicLCDRefreshEvent::getXEnd() const
{
	return m_XEnd;
}

unsigned int MnemonicLCDRefreshEvent::getYEnd() const
{
	return m_YEnd;
}

IMnemonicLCDRefreshEventListener::~IMnemonicLCDRefreshEventListener()
{
	this->unbindFromMnemonicLCDRefreshEventSystem();
}

void IMnemonicLCDRefreshEventListener::bindToMnemonicLCDRefreshEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IMnemonicLCDRefreshEventListener::unbindFromMnemonicLCDRefreshEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IMnemonicLCDRefreshEventListener::PublishEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent)
{
	m_EventDispatcher.dispatch( lcdRefreshEvent );
}
