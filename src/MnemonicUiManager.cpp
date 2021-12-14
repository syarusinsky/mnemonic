#include "MnemonicUiManager.hpp"

#include "Graphics.hpp"

MnemonicUiManager::MnemonicUiManager (unsigned int width, unsigned int height, const CP_FORMAT& format) :
	Surface( width, height, format )
{
}

MnemonicUiManager::~MnemonicUiManager()
{
}

void MnemonicUiManager::setFont (Font* font)
{
	m_Graphics->setFont( font );
}

void MnemonicUiManager::draw()
{
	m_Graphics->setColor( true );
	m_Graphics->fill();
}
