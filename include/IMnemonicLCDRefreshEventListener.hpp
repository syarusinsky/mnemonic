#ifndef IMNEMONICLCDREFRESHEVENTLISTENER_HPP
#define IMNEMONICLCDREFRESHEVENTLISTENER_HPP

/*******************************************************************
 * An IMnemonicLCDRefreshEventListener specifies a simple interface
 * which a subclass can use to be notified of events informing
 * the listener that the LCD (or LCD representation) needs to be
 * refreshed. The events also include the 'dirty' rectangle, in
 * case the only a portion of the lcd needs to be refreshed.
*******************************************************************/

#include "IEventListener.hpp"

class MnemonicLCDRefreshEvent : public IEvent
{
	public:
		MnemonicLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
					unsigned int channel);
		~MnemonicLCDRefreshEvent() override;

		unsigned int getXStart() const;
		unsigned int getYStart() const;
		unsigned int getXEnd() const;
		unsigned int getYEnd() const;

	private:
		unsigned int m_XStart;
		unsigned int m_YStart;
		unsigned int m_XEnd;
		unsigned int m_YEnd;
};

class IMnemonicLCDRefreshEventListener : public IEventListener
{
	public:
		virtual ~IMnemonicLCDRefreshEventListener();

		virtual void onMnemonicLCDRefreshEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent) = 0;

		void bindToMnemonicLCDRefreshEventSystem();
		void unbindFromMnemonicLCDRefreshEventSystem();

		static void PublishEvent (const MnemonicLCDRefreshEvent& lcdRefreshEvent);

	private:
		static EventDispatcher<IMnemonicLCDRefreshEventListener, MnemonicLCDRefreshEvent,
					&IMnemonicLCDRefreshEventListener::onMnemonicLCDRefreshEvent> m_EventDispatcher;
};

#endif // IMNEMONICLCDREFRESHEVENTLISTENER_HPP
