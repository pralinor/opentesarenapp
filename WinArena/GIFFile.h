/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/GIFFile.h 3     12/11/05 11:52a Lee $
//
//	File: GIFFile.cpp
//
//
//	Ancient, rickety, experimental version of GIF encode/decode.  It works.
//	That's about all I can say for it.  But it is independent of my other
//	software libraries, so I can include it without having to weld in tens
//	of thousands of lines of code that have nothing to do with it.
//
/////////////////////////////////////////////////////////////////////////////


#if !defined(APP_GIFFile_h)
#define APP_GIFFile_h


#include <windows.h>
#include "MappedFile.h"
#include "LZWCodec.h"


enum GIFError {
	GIF_Success,
	GIF_NoImage,
	GIF_InvalidPointer,
	GIF_NoFileName,
	GIF_CannotOpenFile,
	GIF_FileNotOpen,
	GIF_ReadFromOutputFile,
	GIF_WriteToInputFile,
	GIF_ReadPastEndOfFile,
	GIF_WritePastEndOfFile,
	GIF_FileIsNotGIF,				// specified file is not a GIF
	GIF_InvalidImageSize,
	GIF_InvalidCodeSize,
	GIF_FileTooSmall,
	GIF_UnknownExtension,
	GIF_UnknownLabel,
	GIF_CodecError
};


typedef struct {
	long  Width;
	long  Height;
	long  Pitch;
	long  BufferSize;
	BYTE* pBuffer;
	BYTE  ColorMap[256][3];
	DWORD ColorMapEntryCount;
} GIFFrameBuffer;


class CGIFFile
{
public:
	CGIFFile(void);
	~CGIFFile(void);

	GIFError Open(char *filename, bool isInput);
	GIFError Close(void);

	GIFError GetInfo(DWORD &width, DWORD &height);
	GIFError ReadImage(GIFFrameBuffer &frame);

	GIFError InitOutput(long width, long height);
	GIFError SetCodeSize(long codeSize);
	DWORD    GetCodeSize(void) { return m_LzwCodeSize; }
	GIFError SetComment(char *pComment);
	GIFError SetGlobalColorMap(long entryCount, BYTE *pColorTable);
	GIFError GetGlobalColorMap(long &entryCount, BYTE *pColorTable);
	GIFError SetDelayTime(DWORD centiseconds);
	GIFError StartAnimation(void);
	GIFError WriteImage(GIFFrameBuffer &frame, long top = 0, long left = 0);
	GIFError FinishOutput(void);

	bool IsOpen(void) { return m_File.IsOpen(); };

private:
	GIFError readFromFile(void *buffer, long size);
	GIFError writeToFile(void *buffer, long size);
	GIFError readHeader(void);
	GIFError decodeStream(GIFFrameBuffer &frame, void *desc);
	GIFError readBlock(BYTE *buffer, BYTE &blockSize);
	GIFError encodeStream(GIFFrameBuffer &imageData, long top, long left, long width, long height);
	GIFError writeControlExtension(void);
	DWORD    highestBit(DWORD value);

	CMappedFile	m_File;
	bool		m_IsInput;
	BYTE*		m_pFileBuffer;
	long		m_FileOffset;
	long		m_FileSize;
	long		m_ImageWidth;
	long		m_ImageHeight;
	BYTE		m_GlobalColorMap[256][3];
	long		m_GlobalColorMapEntryCount;
	char*		m_pCommentBuffer;
	long		m_CommentBufferSize;
	long		m_FrameCount;
	bool		m_IsInterlaced;
	BYTE		m_LzwCodeSize;
	CLZWCodec	m_Codec;
	WORD		m_DelayTime;
};


#endif // APP_GIFFile_h
