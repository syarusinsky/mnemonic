#include "StringEditModel.hpp"

#include <string.h>

StringEditModel::StringEditModel (unsigned int stringLength) :
	m_String{ ' ' },
	m_MaxStringPos( stringLength - 1 ),
	m_CursorPos( 0 ),
	m_EditMode( false )
{
	m_String[m_MaxStringPos + 1] = '\0';
}

StringEditModel::~StringEditModel()
{
}

void StringEditModel::setEditMode (bool editMode)
{
	m_EditMode = editMode;
}

bool StringEditModel::getEditMode() const
{
	return m_EditMode;
}

unsigned int StringEditModel::getCursorPos() const
{
	return m_CursorPos;
}

void StringEditModel::cursorNext()
{
	if ( m_EditMode )
	{
		if ( m_String[m_CursorPos] == ' ' )
		{
			m_String[m_CursorPos] = 'A';
		}
		else if ( m_String[m_CursorPos] == 'Z' )
		{
			m_String[m_CursorPos] = 'a';
		}
		else if ( m_String[m_CursorPos] == 'z' )
		{
			return;
		}
		else
		{
			m_String[m_CursorPos] = m_String[m_CursorPos] + 1;
		}
	}
	else // hover mode
	{
		if ( m_CursorPos < m_MaxStringPos )
		{
			m_CursorPos++;
		}
	}
}

void StringEditModel::cursorPrev()
{
	if ( m_EditMode )
	{
		if ( m_String[m_CursorPos] == 'a' )
		{
			m_String[m_CursorPos] = 'Z';
		}
		else if ( m_String[m_CursorPos] == 'A' )
		{
			m_String[m_CursorPos] = ' ';
		}
		else if ( m_String[m_CursorPos] == ' ' )
		{
			return;
		}
		else
		{
			m_String[m_CursorPos] = m_String[m_CursorPos] - 1;
		}
	}
	else // hover mode
	{
		if ( m_CursorPos > 0 )
		{
			m_CursorPos--;
		}
	}
}

bool StringEditModel::setString (const char* string)
{
	unsigned int length = strlen( string );

	if ( length > m_MaxStringPos + 1 ) return false;

	unsigned int startPos = ( m_MaxStringPos + 1 ) - length;

	// fill the beginning section with spaces
	for ( unsigned int pos = 0; pos < startPos; pos++ )
	{
		m_String[pos] = ' ';
	}

	// fill in the given string
	for ( unsigned int pos = startPos; pos < m_MaxStringPos + 1; pos++ )
	{
		// TODO possibly want to check for characters we don't want the user to be able to use?
		m_String[pos] = string[pos - startPos];
	}

	return true;
}

const char* StringEditModel::getString() const
{
	return m_String;
}
