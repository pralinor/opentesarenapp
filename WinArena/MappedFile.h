//////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MappedFile.h 1     11/08/05 12:06a Lee $
//
//	File: MappedFile.h
//
//	Definition of CMappedFile class.
//
//	The CMappedFile class is used to map a file into application memory so
//	that the file's contents can be accessed as a linear array of bytes.
//
//	A mapped file can be opened as read-only (the default) or read/write.
//	When write access is allowed, existing data can be overwritten, but
//	the size of the file cannot change.
//
//////////////////////////////////////////////////////////////////////////////

#if !defined(APP_MappedFile_h)
#define APP_MappedFile_h


class CMappedFile
{
public:
	CMappedFile(void);
	~CMappedFile(void);

	bool  Open(char *filename, bool writeAccess = false, long createSize = -1);
	bool  Close(long size = -1);
	BYTE* Address(void);
	DWORD Size(void);
	bool  IsOpen(void);
	bool  WriteAccess(void);

private:
	bool MapFile(void);
	bool UnmapFile(long size = -1);

	char	m_FileName[MAX_PATH];
	HANDLE	m_hFile;
	HANDLE	m_hMapping;
	DWORD	m_FileSize;
	BYTE*	m_pMapAdr;
	bool	m_Mapped;
	bool	m_WriteAccess;
	long	m_CreateSize;
};


#endif // APP_MappedFile_h
