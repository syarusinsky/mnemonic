#include "AudioTrack.hpp"

#include "B12Compression.hpp"
#include <cstring>

// TODO remove this after testing
#include <iostream>

constexpr unsigned int COMPRESSED_BUFFER_SIZE = static_cast<unsigned int>( ABUFFER_SIZE * 2.0f * 0.75f );

AudioTrack::AudioTrack (unsigned int b12BufferSize) :
	m_B12BufferSize( b12BufferSize ),
	m_B12CircularBufferSize( m_B12BufferSize * 3 ),
	m_B12CircularBuffer( new uint8_t[m_B12CircularBufferSize] ),
	m_B12WritePos( 0 ),
	m_B12ReadPos( 0 ),
	m_DecompressedBuffer{ 0 }
{
	for ( int byte = 0; byte < m_B12CircularBufferSize; byte++ )
	{
		m_B12CircularBuffer[byte] = 0;
	}
}

AudioTrack::~AudioTrack()
{
	delete[] m_B12CircularBuffer;
}

void AudioTrack::reset()
{
	std::cout << "m_B12WritePos: " << std::to_string(m_B12WritePos) << std::endl;
	std::cout << "m_B12ReadPos: " << std::to_string(m_B12ReadPos) << std::endl;

	m_B12WritePos = 0;
	m_B12ReadPos = 0;

	for ( int byte = 0; byte < m_B12CircularBufferSize; byte++ )
	{
		m_B12CircularBuffer[byte] = 0;
	}
}

bool AudioTrack::shouldFillNextBuffer()
{
	if ( m_B12ReadPos <= m_B12WritePos && m_B12ReadPos + COMPRESSED_BUFFER_SIZE >= m_B12WritePos )
	{
		return true;
	}
	else if ( m_B12ReadPos > m_B12WritePos && m_B12WritePos + m_B12BufferSize < m_B12ReadPos )
	{
		return true;
	}

	return false;
}

void AudioTrack::fillNextBuffer (const uint8_t* const compressedBuf)
{
	uint8_t* nextBuffer = &m_B12CircularBuffer[m_B12WritePos];

	std::memcpy( nextBuffer, compressedBuf, m_B12BufferSize );

	m_B12WritePos = ( m_B12WritePos + m_B12BufferSize ) % m_B12CircularBufferSize;
}

void AudioTrack::call (uint16_t* writeBuffer)
{
	uint8_t* compressedBuffer = &m_B12CircularBuffer[m_B12ReadPos];

	if ( ! B12Decompress( compressedBuffer, COMPRESSED_BUFFER_SIZE, m_DecompressedBuffer, ABUFFER_SIZE ) )
	{
		std::cout << "DECOMPRESSION ERROR: " << std::to_string(ABUFFER_SIZE) << " for decompressed buffer size and "
			<< std::to_string(COMPRESSED_BUFFER_SIZE) << " for compressed buffer size" << std::endl;
	}

	std::memcpy( writeBuffer, m_DecompressedBuffer, ABUFFER_SIZE * sizeof(uint16_t) );

	m_B12ReadPos = ( m_B12ReadPos + COMPRESSED_BUFFER_SIZE ) % m_B12CircularBufferSize;
}
