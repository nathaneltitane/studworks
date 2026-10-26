#include "sw_global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sw_file.h"

swMemoryFile::swMemoryFile()
{
	mGrowBytes = 1024;
	mPosition = 0;
	mBufferSize = 0;
	mFileSize = 0;
	mBuffer = nullptr;
}

swMemoryFile::~swMemoryFile()
{
	swMemoryFile::Close();
}

void swMemoryFile::Seek(qint64 Offset, int From)
{
	if (From == SEEK_SET)
		mPosition = Offset;
	else if (From == SEEK_CUR)
		mPosition += Offset;
	else if (From == SEEK_END)
		mPosition = mFileSize + Offset;
}

long swMemoryFile::GetPosition() const
{
	return (long)mPosition;
}

void swMemoryFile::SetLength(size_t NewLength)
{
	if (NewLength > mBufferSize)
		GrowFile(NewLength);

	if (NewLength < mPosition)
		mPosition = NewLength;

	mFileSize = NewLength;
}

size_t swMemoryFile::GetLength() const
{
	return mFileSize;
}

void swMemoryFile::Close()
{
	if (!mBuffer)
		return;

	mPosition = 0;
	mBufferSize = 0;
	mFileSize = 0;
	free(mBuffer);
	mBuffer = nullptr;
}

size_t swMemoryFile::ReadBuffer(void* Buffer, size_t Bytes)
{
	if (Bytes == 0 || mPosition > mFileSize)
		return 0;

	size_t BytesToRead;

	if (mPosition + Bytes > mFileSize)
		BytesToRead = mFileSize - mPosition;
	else
		BytesToRead = Bytes;

	memcpy(Buffer, mBuffer + mPosition, BytesToRead);
	mPosition += BytesToRead;

	return BytesToRead;
}

size_t swMemoryFile::WriteBuffer(const void* Buffer, size_t Bytes)
{
	if (Bytes == 0)
		return 0;

	if (mPosition + Bytes > mBufferSize)
		GrowFile(mPosition + Bytes);

	memcpy(mBuffer + mPosition, Buffer, Bytes);

	mPosition += Bytes;

	if (mPosition > mFileSize)
		mFileSize = mPosition;

	return Bytes;
}

void swMemoryFile::GrowFile(size_t NewLength)
{
	if (NewLength <= mBufferSize)
		return;

	NewLength = ((NewLength + mGrowBytes - 1) / mGrowBytes) * mGrowBytes;

	if (mBuffer)
	{
		unsigned char* NewBuffer = (unsigned char*)realloc(mBuffer, NewLength);

		if (!NewBuffer)
			return;

		mBuffer = NewBuffer;
	}
	else
		mBuffer = (unsigned char*)malloc(NewLength);

	mBufferSize = NewLength;
}

char* swMemoryFile::ReadLine(char* Buffer, size_t BufferSize)
{
	int BytesRead = 0;
	unsigned char ch;

	if (BufferSize == 0)
		return nullptr;

	if (mPosition >= mFileSize)
		return nullptr;

	while ((--BufferSize))
	{
		if (mPosition == mFileSize)
			break;

		ch = mBuffer[mPosition];
		mPosition++;
		Buffer[BytesRead++] = ch;

		if (ch == '\n')
			break;
	}

	Buffer[BytesRead] = 0;
	return Buffer;
}
