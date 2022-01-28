#include "AudioTrack.hpp"

#include "B12Compression.hpp"
#include "IAllocator.hpp"
#include "Fat16FileManager.hpp"
#include <cstring>

constexpr unsigned int COMPRESSED_BUFFER_SIZE = static_cast<unsigned int>( ABUFFER_SIZE * 2.0f * 0.75f );

AudioTrack::AudioTrack (Fat16FileManager& fileManager, const Fat16Entry& entry, unsigned int b12BufferSize, IAllocator& allocator,
			uint16_t* decompressedBuffer) :
	m_FileManager( fileManager ),
	m_FatEntry( entry ),
	m_Allocator( allocator ),
	m_B12BufferSize( b12BufferSize ),
	m_B12CircularBufferSize( m_B12BufferSize * 3 ),
	m_B12CircularBuffer( m_Allocator.allocatePrimativeArray<uint8_t>(m_B12CircularBufferSize) ),
	m_B12WritePos( 0 ),
	m_B12ReadPos( 0 ),
	m_DecompressedBuffer( decompressedBuffer )
{
	for ( int byte = 0; byte < m_B12CircularBufferSize; byte++ )
	{
		m_B12CircularBuffer[byte] = 0;
	}
}

AudioTrack::~AudioTrack()
{
}

bool AudioTrack::operator== (const AudioTrack& other) const
{
	if ( strcmp(m_FatEntry.getFilenameDisplay(), other.getFatEntry().getFilenameDisplay()) == 0 )
	{
		return true;
	}

	return false;
}

void AudioTrack::play()
{
	this->reset();

	m_FileManager.readEntry( m_FatEntry );
}

void AudioTrack::reset()
{
	m_FatEntry.getFileTransferInProgressFlagRef() = false;

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

void AudioTrack::call (int16_t* writeBuffer)
{
	if ( m_FatEntry.getFileTransferInProgressFlagRef() )
	{
		while ( this->shouldFillNextBuffer() )
		{
			SharedData<uint8_t> data = m_FileManager.getSelectedFileNextSector( m_FatEntry );
			if ( m_FatEntry.getFileTransferInProgressFlagRef() || data.getPtr() != nullptr )
			{
				this->fillNextBuffer( &data[0] );
			}
			else
			{
				break;
			}
		}

	}

	if ( this->shouldDecompress() )
	{
		this->decompressToBuffer( writeBuffer );
	}
}

bool AudioTrack::shouldDecompress()
{
	if ( m_B12ReadPos < m_B12WritePos && m_B12ReadPos + COMPRESSED_BUFFER_SIZE <= m_B12WritePos )
	{
		return true;
	}
	else if ( m_B12ReadPos > m_B12WritePos && m_B12CircularBufferSize - m_B12ReadPos + m_B12WritePos >= COMPRESSED_BUFFER_SIZE )
	{
		return true;
	}

	return false;
}

void AudioTrack::decompressToBuffer (int16_t* writeBuffer)
{
	uint8_t* compressedBuffer = &m_B12CircularBuffer[m_B12ReadPos];

	B12Decompress( compressedBuffer, COMPRESSED_BUFFER_SIZE, m_DecompressedBuffer, ABUFFER_SIZE );

	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBuffer[sample] += ( static_cast<int16_t>(m_DecompressedBuffer[sample]) - (4096 / 2) ) / 2;
	}

	m_B12ReadPos = ( m_B12ReadPos + COMPRESSED_BUFFER_SIZE ) % m_B12CircularBufferSize;
}

void AudioTrack::freeData()
{
	m_Allocator.free( m_B12CircularBuffer );
	m_Allocator.free( m_DecompressedBuffer );
}
