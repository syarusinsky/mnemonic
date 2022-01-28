#ifndef IMNEMONICUIEVENTLISTENER_HPP
#define IMNEMONICUIEVENTLISTENER_HPP

/*******************************************************************
 * An IMnemonicUiEventListener specifies a simple interface which
 * a subclass can use to be notified of mnemonic ui events. These
 * events can include error notifications.
*******************************************************************/

#include "IEventListener.hpp"
#include "Fat16Entry.hpp"

enum class UiEventType : unsigned int
{
	INVALID_FILESYSTEM, 	// for when the sd card isn't found or the file system isn't fat16
	ENTER_STATUS_PAGE,
	ENTER_FILE_EXPLORER,
};

struct UiFileExplorerEntry
{
	char 		m_FilenameDisplay[FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2]; // plus 2 for . and string terminator
	unsigned int 	m_Index; // the index in the directory
};

class MnemonicUiEvent : public IEvent
{
	public:
		MnemonicUiEvent (const UiEventType& eventType, void* dataPtr, unsigned int dataNumElements, unsigned int channel);
		~MnemonicUiEvent() override;

		UiEventType getEventType() const { return m_EventType; }

		void* getDataPtr() const { return m_DataPtr; }

		unsigned int getDataNumElements() const { return m_DataNumElements; }

	private:
		UiEventType 	m_EventType;
		void* 		m_DataPtr;
		unsigned int 	m_DataNumElements;
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
