//////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MappedFile.cpp 1     11/08/05 12:06a Lee $
//
//	File: MappedFile.cpp
//
//	Implementation of CMappedFile class.
//
//	The CMappedFile class is used to map a file into application memory so
//	that the file's contents can be accessed as a linear array of bytes.
//
//	A mapped file can be opened as read-only (the default) or read/write.
//	When write access is allowed, existing data can be overwritten, but
//	the size of the file cannot change.
//
//////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "MappedFile.h"


//////////////////////////////////////////////////////////////////////////////
//
CMappedFile::CMappedFile(void)
	:	m_hFile(INVALID_HANDLE_VALUE),
		m_hMapping(0),
		m_FileSize(0),
		m_pMapAdr(0),
		m_Mapped(false),
		m_WriteAccess(false),
		m_CreateSize(-1)
{
}


//////////////////////////////////////////////////////////////////////////////
//
CMappedFile::~CMappedFile(void)
{
	UnmapFile();
}


//////////////////////////////////////////////////////////////////////////////
//
//	Open()
//
bool CMappedFile::Open(char *filename, bool writeAccess, long createSize)
{
	Close();

	if (!filename || (filename[0] == '\0'))
		return false;

	long length = long(strlen(filename));
	if (length >= MAX_PATH)
		length = MAX_PATH - 1;

	memcpy(m_FileName, filename, length);
	m_FileName[length] = '\0';

	m_WriteAccess = writeAccess;
	m_CreateSize  = createSize;

	return MapFile();
}


//////////////////////////////////////////////////////////////////////////////
//
//	Close()
//
bool CMappedFile::Close(long size)
{
	m_WriteAccess = false;

	return UnmapFile(size);
}


//////////////////////////////////////////////////////////////////////////////
//
//	Address()
//
BYTE* CMappedFile::Address(void)
{
	return (m_Mapped ? m_pMapAdr : 0);
}


//////////////////////////////////////////////////////////////////////////////
//
//	Size()
//
DWORD CMappedFile::Size(void)
{
	return m_FileSize;
}


//////////////////////////////////////////////////////////////////////////////
//
//	IsOpen()
//
//	Returns true if a file is presently open.
//
bool CMappedFile::IsOpen(void)
{
	return m_Mapped;
}


//////////////////////////////////////////////////////////////////////////////
//
//	WriteAccess()
//
//	Returns true if the file is open and it is writable.
//
bool CMappedFile::WriteAccess(void)
{
	return (IsOpen() && m_WriteAccess);
}


//////////////////////////////////////////////////////////////////////////////
//
//	MapFile()
//
bool CMappedFile::MapFile(void)
{
	UnmapFile();

	DWORD fileAccess = m_WriteAccess ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
	DWORD protection = m_WriteAccess ? PAGE_READWRITE : PAGE_READONLY;
	DWORD mapAccess  = m_WriteAccess ? FILE_MAP_WRITE : FILE_MAP_READ;
	DWORD creation   = (m_WriteAccess && (m_CreateSize > 0)) ? CREATE_ALWAYS : OPEN_EXISTING;
	DWORD attribute  = FILE_ATTRIBUTE_NORMAL;

	// Try to open the file.
	m_hFile = CreateFile(m_FileName, fileAccess, FILE_SHARE_READ, NULL, creation, attribute, NULL);
	if (m_hFile != INVALID_HANDLE_VALUE) {

		// Fetch the file size, and make certain it is small enough to
		// stand a chance of being successfully mapped into memory.
		DWORD highSize = 0;
		if (m_CreateSize > 0)
			m_FileSize = m_CreateSize;
		else
			m_FileSize = GetFileSize(m_hFile, &highSize);

		if ((m_FileSize == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) {
			// A system error occurred.
			UnmapFile();
		}
		else if ((highSize != 0) || (m_FileSize >= 0x80000000)) {
			// File too large.
			UnmapFile();
		}
		else if (m_FileSize == 0) {
			// File is empty.
			UnmapFile();
		}
		else {
			// Attempt to open a file mapping.
			m_hMapping = CreateFileMapping(m_hFile, NULL, protection, 0, m_FileSize, NULL);
			if (m_hMapping != 0) {

				// Finally, map the file into virtual memory.
				m_pMapAdr = (BYTE*)MapViewOfFile(m_hMapping, mapAccess, 0, 0, m_FileSize);
				if (m_pMapAdr != 0)
					m_Mapped = true;
				else {
					// A system error occurred.
					UnmapFile();
				}
			}
			else {
				// A system error occurred.
				UnmapFile();
			}
		}
	}
	else {
		// A system error occurred.
	}

	return m_Mapped;
}


//////////////////////////////////////////////////////////////////////////////
//
//	UnmapFile()
//
bool CMappedFile::UnmapFile(long size)
{
	// Unmap the file from memory.
	if (m_pMapAdr) {
		UnmapViewOfFile((void*)m_pMapAdr);
		m_pMapAdr = 0;
	}

	// Close the file mapping.
	if (m_hMapping != 0) {
		CloseHandle(m_hMapping);
		m_hMapping = 0;
	}

	// Close the file itself.
	if (m_hFile != INVALID_HANDLE_VALUE) {

		// If a fixed file size has been specified, move the current
		// file pointer to that location, then clip the file.
		if (size > 0) {
			SetFilePointer(m_hFile, size, 0, FILE_BEGIN);
			SetEndOfFile(m_hFile);
		}
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}

	m_FileSize = 0;
	m_Mapped   = false;

	return true;
}
