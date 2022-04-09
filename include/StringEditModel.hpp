#ifndef STRINGEDITMODEL_HPP
#define STRINGEDITMODEL_HPP

/*************************************************************************
 * The StringEditModel is a simple class for editing the individual
 * characters of a string of a given length. It has a cursor which points
 * to the character currently being edited, which has two states. The
 * first state of the cursor is the 'hover' mode, where the cursor can
 * be moved from character position to character position. The second
 * state of the cursor is the 'edit' mode, where the cursor can change
 * the character at the hovered character position.
*************************************************************************/

#define MAX_LENGTH 8

class StringEditModel
{
	public:
		StringEditModel (unsigned int stringLength = MAX_LENGTH);
		~StringEditModel();

		// either in edit mode, or hover mode
		void setEditMode (bool editMode);
		bool getEditMode() const;

		unsigned int getCursorPos() const;

		// both either move cursor position or change characters at cursor position
		void cursorNext();
		void cursorPrev();

		bool setString (const char* string); // MUST BE NULL-TERMINATED C STRING
		const char* getString() const;

	private:
		char 		m_String[MAX_LENGTH + 1];
		unsigned int 	m_MaxStringPos;

		unsigned int 	m_CursorPos;

		bool 		m_EditMode; // true for edit mode, false for hover mode
};

#endif // STRINGEDITMODEL_HPP
