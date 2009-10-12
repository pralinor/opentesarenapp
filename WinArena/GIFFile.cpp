/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/GIFFile.cpp 3     12/11/05 11:52a Lee $
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


#include <windows.h>

#include "GIFFile.h"



#pragma pack(push, normalPacking)
#pragma pack(1)

const BYTE GIFGlobalMapPresent	= 0x80;
const BYTE GIFGlobalMapSorted	= 0x08;
const BYTE GIFLocalMapPresent	= 0x80;
const BYTE GIFInterlaced		= 0x40;
const BYTE GIFColorMapSorted	= 0x20;
const BYTE GIFTransparentImage	= 0x01;

typedef struct tagGIFScreenDescriptor {
	WORD	Width;
	WORD	Height;
	BYTE	PackedFields;
	BYTE	BackgroundIndex;
	BYTE	AspectRatio;
} GIFScreenDescriptor;

typedef struct tagGIFImageDescriptor {
	WORD	LeftPosition;
	WORD	TopPosition;
	WORD	Width;
	WORD	Height;
	BYTE	PackedFields;
} GIFImageDescriptor;

typedef struct tagGIFTextExtension {
	BYTE	BlockSize;
	WORD	LeftPosition;
	WORD	TopPosition;
	WORD	Width;
	WORD	Height;
	BYTE	CellWidth;
	BYTE	CellHeight;
	BYTE	ForegroundIndex;
	BYTE	BackgroundIndex;
} GIFTextExtension;

typedef struct tagGIFControlExtension {
	BYTE	BlockSize;
	BYTE	PackedFields;
	WORD	DelayTime;
	BYTE	TransparentColorIndex;
	BYTE	BlockTerminator;
} GIFControlExtension;

typedef struct tagGIFApplicationExtension {
	BYTE	BlockSize;
	BYTE	Identifier[8];
	BYTE	Authentication[3];
} GIFApplicationExtension;

#pragma pack(pop, normalPacking)



//////////////////////////////////////////////////////////////////////////////

class CGIFEncodeToMemory : public CLZWOutput
{
public:
	CGIFEncodeToMemory(void);
	~CGIFEncodeToMemory(void);

	void     SetMemory(BYTE *buffer, DWORD size);
	DWORD    DataSize(void);
	LZWError Initialize(void);
	LZWError WriteBuffer(const BYTE *buffer, const DWORD byteCount);
	LZWError CleanUp(void);

private:
	BYTE*	m_pBuffer;
	DWORD	m_Offset;
	DWORD	m_Size;
};


CGIFEncodeToMemory::CGIFEncodeToMemory(void)
	:	m_pBuffer(0),
		m_Offset(0),
		m_Size(0)
{
}


CGIFEncodeToMemory::~CGIFEncodeToMemory(void)
{
}


void CGIFEncodeToMemory::SetMemory(BYTE *buffer, DWORD size)
{
	m_pBuffer = buffer;
	m_Size    = size;
}


DWORD CGIFEncodeToMemory::DataSize(void)
{
	return m_Offset;
}


LZWError CGIFEncodeToMemory::Initialize(void)
{
	m_Offset = 0;

	return LZW_Success;
}


LZWError CGIFEncodeToMemory::WriteBuffer(const BYTE *buffer, const DWORD byteCount)
{
	BYTE count;
	DWORD remaining = byteCount;
	BYTE *buf = (BYTE*)buffer;

	while (remaining) {
		if (remaining > 255)
			count = 255;
		else
			count = BYTE(remaining);

		if ((DWORD(count) + 1 + m_Offset) > m_Size)
			return LZW_OutputError;

		m_pBuffer[m_Offset++] = count;
		memcpy(m_pBuffer + m_Offset, buf, DWORD(count));
		m_Offset	+= count;
		buf			+= DWORD(count);
		remaining	-= DWORD(count);
	}

	return LZW_Success;
}


LZWError CGIFEncodeToMemory::CleanUp(void)
{
	m_pBuffer[m_Offset++] = 0;

	return LZW_Success;
}



//////////////////////////////////////////////////////////////////////////////

class CGIFDecodeFromMemory : public CLZWOutput
{
public:
	CGIFDecodeFromMemory(void);
	~CGIFDecodeFromMemory(void);

	void     SetMemory(BYTE *buffer, DWORD size, DWORD pitch);
	void     SetBounds(long left, long top, long width, long height);
	void     SetInterlacing(bool enable);
	LZWError Initialize(void);
	LZWError WriteBuffer(const BYTE *buffer, const DWORD byteCount);
	LZWError CleanUp(void);

private:
	long			m_Left;
	long			m_Top;
	long			m_Right;
	long			m_Bottom;
	long			m_Height;
	long			m_LineNum;
	long			m_PixelNum;
	BYTE*			m_pScanLine;
	bool			m_IsInterlaced;
	long			m_InterlaceFirst;
	long			m_InterlaceIncrement;
	BYTE*			m_pBuffer;
	DWORD			m_Size;
	DWORD			m_Pitch;
};


CGIFDecodeFromMemory::CGIFDecodeFromMemory(void)
	:	m_pBuffer(0),
		m_IsInterlaced(false),
		m_InterlaceFirst(0)
{
}


CGIFDecodeFromMemory::~CGIFDecodeFromMemory(void)
{
}


void CGIFDecodeFromMemory::SetMemory(BYTE *buffer, DWORD size, DWORD pitch)
{
	m_pBuffer	= buffer;
	m_Size		= size;
	m_Pitch		= pitch;
}


void CGIFDecodeFromMemory::SetBounds(long left, long top, long width, long height)
{
	m_Left		= left;
	m_Top		= top;
	m_Right		= left + width;
	m_Bottom	= top + height;
	m_Height	= height;
}


void CGIFDecodeFromMemory::SetInterlacing(bool enable)
{
	m_IsInterlaced = enable;
	if (m_IsInterlaced) {
		m_InterlaceFirst		= 8;
		m_InterlaceIncrement	= 8;
	}
}

LZWError CGIFDecodeFromMemory::Initialize(void)
{
	m_LineNum	= 0;
	m_PixelNum	= m_Left;
	m_pScanLine	= m_pBuffer + (m_LineNum + m_Top) * m_Pitch;

	return LZW_Success;
}


LZWError CGIFDecodeFromMemory::WriteBuffer(const BYTE *buffer, const DWORD byteCount)
{
	DWORD index;
	for (index = 0; index < byteCount; ++index) {
		if (m_LineNum < m_Height)
			m_pScanLine[m_PixelNum++] = *(buffer++);
		if (m_PixelNum >= m_Right) {
			if (m_IsInterlaced) {
				m_LineNum += m_InterlaceIncrement;
				if (m_LineNum >= m_Height) {
					if (m_InterlaceFirst == 8)
						m_InterlaceFirst = 4;
					else if (m_InterlaceFirst == 4) {
						m_InterlaceFirst = 2;
						m_InterlaceIncrement = 4;
					}
					else if (m_InterlaceFirst == 2) {
						m_InterlaceFirst = 1;
						m_InterlaceIncrement = 2;
					}
					m_LineNum = m_InterlaceFirst;
				}
			}
			else
				++m_LineNum;

			m_PixelNum = m_Left;
			m_pScanLine	= m_pBuffer + (m_LineNum + m_Top) * m_Pitch;
		}
	}

	return LZW_Success;
}


LZWError CGIFDecodeFromMemory::CleanUp(void)
{
	return LZW_Success;
}



//////////////////////////////////////////////////////////////////////////////


CGIFFile::CGIFFile()
	:	m_IsInput(false),
		m_pFileBuffer(0),
		m_FileOffset(0),
		m_FileSize(0),
		m_pCommentBuffer(0),
		m_CommentBufferSize(0),
		m_LzwCodeSize(0),
		m_DelayTime(0),
		m_IsInterlaced(0)
{
}

CGIFFile::~CGIFFile()
{
	if (m_pCommentBuffer)
		delete [] m_pCommentBuffer;
}


GIFError CGIFFile::Open(char *filename, bool isInput)
{
	Close();

	if ((filename == 0) || (filename[0] == '\0'))
		return GIF_NoFileName;

	long createSize = -1;
	if (!isInput)
		createSize = 1024 * 1024;

	if (!m_File.Open(filename, !isInput, createSize))
		return GIF_CannotOpenFile;

	m_IsInput     = isInput;
	m_pFileBuffer = m_File.Address();
	m_FileOffset  = 0;
	m_FileSize    = m_File.Size();
	m_FrameCount  = 0;

	if (isInput)
		return readHeader();

	return GIF_Success;
}


GIFError CGIFFile::Close(void)
{
	if (m_File.IsOpen()) {
		if (m_IsInput)
			m_File.Close(-1);
		else
			m_File.Close(m_FileOffset);
	}

	return GIF_Success;
}



GIFError CGIFFile::GetInfo(DWORD &width, DWORD &height)
{
	if (!m_File.IsOpen())
		return GIF_FileNotOpen;
	if (!m_IsInput)
		return GIF_ReadFromOutputFile;

	width  = m_ImageWidth;
	height = m_ImageHeight;

	return GIF_Success;
}


GIFError CGIFFile::ReadImage(GIFFrameBuffer &frame)
{
	GIFImageDescriptor	imageDesc;
	GIFError            status;

	BYTE blockSize;
	BYTE blockData[256];
	BYTE extension;

	bool done = false;
	BYTE label;

	while (!done) {
		status = readFromFile(&label, 1);
		if (status != GIF_Success)
			return status;

		switch (label) {
			case 0x2C:		// image descriptor
				status = readFromFile(&imageDesc, sizeof(imageDesc));
				if (status != GIF_Success)
					return status;

				if (imageDesc.PackedFields & GIFLocalMapPresent) {
					DWORD bitShift = (imageDesc.PackedFields & 0x07) + 1;
					frame.ColorMapEntryCount = 1 << bitShift;
					status = readFromFile(frame.ColorMap, 3 * frame.ColorMapEntryCount);
					if (status != GIF_Success)
						return status;
				}
				else {
					frame.ColorMapEntryCount = m_GlobalColorMapEntryCount;
					memcpy(frame.ColorMap, m_GlobalColorMap, m_GlobalColorMapEntryCount * 3);
				}

				// If the image is interlaced, then set these values
				// in preparation of calculating the proper offsets
				// for each line as it is read in.
				if (imageDesc.PackedFields & GIFInterlaced)
					m_IsInterlaced = true;
				else
					m_IsInterlaced = false;

				// Return the result of decoding, which should be GIF_Success,
				// to indicate that an image was successfully read.
				return decodeStream(frame, &imageDesc);

			case 0x21:		// extension block
				status = readFromFile(&extension, 1);
				if (status != GIF_Success)
					return status;

				switch (extension) {
					case 0x01:			//  plain text extension
						GIFTextExtension textExtension;
						status = readFromFile(&textExtension, sizeof(textExtension));
						if (status != GIF_Success)
							return status;

						do {
							status = readBlock(blockData, blockSize);
							if (status != GIF_Success)
								return status;
						} while (blockSize);
						break;

					case 0xF9:			// graphic control extension
						GIFControlExtension controlExtension;
						status = readFromFile(&controlExtension, sizeof(controlExtension));
						if (status != GIF_Success)
							return status;
						break;

					case 0xFE:			// comment extension
						do {
							status = readFromFile(&blockSize, 1);
							if (status != GIF_Success)
								return status;

							if (blockSize) {
								status = readFromFile(blockData, blockSize);
								if (status != GIF_Success)
									return status;
								blockData[blockSize] = '\0';
							}
						} while (blockSize);
						break;

					case 0xFF:			// application extension
						GIFApplicationExtension appExt;
						status = readFromFile(&appExt, sizeof(appExt));
						if (status != GIF_Success)
							return status;

						do {
							status = readFromFile(&blockSize, 1);
							if (status != GIF_Success)
								return status;
							if (blockSize) {
								status = readFromFile(blockData, blockSize);
								if (status != GIF_Success)
									return status;
							}
						} while (blockSize);
						break;

					default:
						return GIF_UnknownExtension;
				}
				break;

			// In the case of the file trailer, we know that we have reached the
			// end of the GIF file.  Return false to indicate that we are not able
			// to read any more images from the file.
			case 0x3B:		// trailer
				return GIF_NoImage;

			// If label is not recognized, then the file is either corrupt
			// or has some application-specific data present.  In either
			// case, abort any further reading.
			default:
				return GIF_UnknownLabel;
		}
	}

	return GIF_NoImage;
}


GIFError CGIFFile::InitOutput(long width, long height)
{
	if (!m_File.IsOpen())
		return GIF_FileNotOpen;
	if (m_IsInput)
		return GIF_WriteToInputFile;

	m_ImageWidth  = width;
	m_ImageHeight = height;

	GIFError status;

	status = writeToFile("GIF87a", 6);
	if (status != GIF_Success)
		return status;

	GIFScreenDescriptor screen;

	screen.Width			= WORD(m_ImageWidth);
	screen.Height			= WORD(m_ImageHeight);
	screen.PackedFields		= 0;
	if (m_GlobalColorMapEntryCount > 0) {
		DWORD highest = highestBit(m_GlobalColorMapEntryCount);
		if (highest < 3)
			highest = 1;
		else
			highest -= 2;
		screen.PackedFields	|= (BYTE)GIFGlobalMapPresent;
		screen.PackedFields	|= highest;					// color table size
		screen.PackedFields	|= highest << 4;			// color resolution
	}
	screen.BackgroundIndex	= 0;
	screen.AspectRatio		= 0;

	status = writeToFile(&screen, sizeof(screen));
	if (status != GIF_Success)
		return status;

	if (m_GlobalColorMapEntryCount > 0) {
		status = writeToFile(m_GlobalColorMap, m_GlobalColorMapEntryCount * 3);
		if (status != GIF_Success)
			return status;
	}

	if (m_pCommentBuffer != 0) {
		BYTE extension = 0x21;
		status = writeToFile(&extension, sizeof(extension));
		if (status != GIF_Success)
			return status;

		BYTE commentExtension = 0xFE;
		status = writeToFile(&commentExtension, sizeof(commentExtension));
		if (status != GIF_Success)
			return status;

		BYTE comLen;
		int  commentLength = m_CommentBufferSize;
		int  commentOffset = 0;
		do {
			if (commentLength > 255)
				comLen = BYTE(255);
			else
				comLen = BYTE(commentLength);

			status = writeToFile(&comLen, sizeof(comLen));
			if (status != GIF_Success)
				return status;

			status = writeToFile(&(m_pCommentBuffer[commentOffset]), comLen);
			if (status != GIF_Success)
				return status;

			commentLength -= comLen;
			commentOffset += comLen;
		} while (commentLength > 0);

		comLen = 0;
		status = writeToFile(&comLen, sizeof(comLen));
		if (status != GIF_Success)
			return status;
	}

	return GIF_Success;
}


GIFError CGIFFile::SetCodeSize(long codeSize)
{
	if (codeSize > 8)
		return GIF_InvalidCodeSize;
	else if (codeSize < 2)
		codeSize = 2;

	m_LzwCodeSize = BYTE(codeSize);

	return GIF_Success;
}


GIFError CGIFFile::SetComment(char *pComment)
{
	if (m_pCommentBuffer) {
		delete [] m_pCommentBuffer;
		m_pCommentBuffer = 0;
	}

	if ((pComment == 0) || (pComment[0] == '\0'))
		return GIF_Success;

	m_CommentBufferSize = long(strlen(pComment) + 1);
	m_pCommentBuffer    = new char[m_CommentBufferSize];
	memcpy(m_pCommentBuffer, pComment, m_CommentBufferSize);

	return GIF_Success;
}


GIFError CGIFFile::SetGlobalColorMap(long entryCount, BYTE *pColorTable)
{
	if (pColorTable == 0)
		return GIF_InvalidPointer;

	if (entryCount <= 0)
		entryCount = 0;
	else if (entryCount < 4)
		entryCount = 4;
	else if (entryCount > 256)
		entryCount = 256;

	m_GlobalColorMapEntryCount = entryCount;

	if (m_GlobalColorMapEntryCount > 0)
		memcpy(m_GlobalColorMap, pColorTable, (m_GlobalColorMapEntryCount * 3));

	return GIF_Success;
}


GIFError CGIFFile::GetGlobalColorMap(long &entryCount, BYTE *pColorTable)
{
	if (pColorTable == 0)
		return GIF_InvalidPointer;

	entryCount = m_GlobalColorMapEntryCount;
	memcpy(pColorTable, m_GlobalColorMap, (m_GlobalColorMapEntryCount * 3));

	return GIF_Success;
}


GIFError CGIFFile::SetDelayTime(DWORD centiseconds)
{
	m_DelayTime = WORD(centiseconds);

	return GIF_Success;
}


GIFError CGIFFile::StartAnimation(void)
{
	writeControlExtension();

	return GIF_Success;
}


GIFError CGIFFile::WriteImage(GIFFrameBuffer &frame, long top, long left)
{
	if (frame.pBuffer == 0)
		return GIF_InvalidPointer;

	if (m_IsInput)
		return GIF_WriteToInputFile;

	if (top < 0)
		top = 0;
	if (left < 0)
		left = 0;

	WORD width  = WORD(frame.Width);
	WORD height = WORD(frame.Height);

	if (width > (m_ImageWidth - left))
		width = WORD(m_ImageWidth - left);
	if (height > (m_ImageHeight - top))
		height = WORD(m_ImageHeight - top);

	if ((width < 1) || (height < 1))
		return GIF_InvalidImageSize;

	// When there are multiple frames, there needs to be a control extension
	// between each frame to indicate the duration of the delay between
	// displaying the sequential frames.
	if (m_FrameCount > 0)
		writeControlExtension();

	GIFError status;

	BYTE imageDescriptor = 0x2C;
	status = writeToFile(&imageDescriptor, sizeof(imageDescriptor));
	if (status != GIF_Success)
		return status;

	GIFImageDescriptor	image;
	image.LeftPosition	= 0;
	image.TopPosition	= 0;
	image.Width			= width;
	image.Height		= height;
	image.PackedFields	= 0;
	if (frame.ColorMapEntryCount > 0) {
		DWORD highest = highestBit(frame.ColorMapEntryCount);
		if (highest < 3)
			highest = 1;
		else
			highest -= 2;
		image.PackedFields	|= GIFLocalMapPresent;
		image.PackedFields	|= highest;				// color table size
	}
	status = writeToFile(&image, sizeof(image));
	if (status != GIF_Success)
		return status;

	if (frame.ColorMapEntryCount > 0) {
		DWORD entryCount = frame.ColorMapEntryCount;
		if (entryCount > 256)
			entryCount = 256;

		status = writeToFile(frame.ColorMap, entryCount * 3);
		if (status != GIF_Success)
			return status;
	}

	status = encodeStream(frame, top, left, width, height);
	if (status != GIF_Success)
		return status;

	++m_FrameCount;

	return GIF_Success;
}


GIFError CGIFFile::FinishOutput(void)
{
	if (!m_File.IsOpen())
		return GIF_FileNotOpen;
	if (m_IsInput)
		return GIF_WriteToInputFile;

	if (m_FrameCount > 1)
		writeControlExtension();

	BYTE trailor = 0x3B;
	GIFError status = writeToFile(&trailor, sizeof(trailor));
	if (status != GIF_Success)
		return status;

	// Multi-frame GIFs are not supported by the 87a version, so they need
	// to be tagged as 89a.
	if (m_FrameCount > 1)
		memcpy(m_pFileBuffer, "GIF89a", 6);

	m_File.Close(m_FileOffset);

	return status;
}


GIFError CGIFFile::readFromFile(void *buffer, long size)
{
	if (!m_File.IsOpen())
		return GIF_FileNotOpen;
	if (!m_IsInput)
		return GIF_ReadFromOutputFile;
	if (!buffer || (size < 0))
		return GIF_InvalidPointer;
	if ((size + m_FileOffset) > m_FileSize)
		return GIF_ReadPastEndOfFile;

	memcpy(buffer, m_pFileBuffer + m_FileOffset, size);
	m_FileOffset += size;

	return GIF_Success;
}


GIFError CGIFFile::writeToFile(void *buffer, long size)
{
	if (!m_File.IsOpen())
		return GIF_FileNotOpen;
	if (m_IsInput)
		return GIF_WriteToInputFile;
	if (!buffer || (size < 0))
		return GIF_InvalidPointer;
	if ((size + m_FileOffset) > m_FileSize)
		return GIF_WritePastEndOfFile;

	memcpy(m_pFileBuffer + m_FileOffset, buffer, size);
	m_FileOffset += size;

	return GIF_Success;
}


GIFError CGIFFile::readHeader(void)
{
	GIFError status;

	///////////////////////////////
	//	Read the GIF signature.  //
	///////////////////////////////

	// Read in the GIF file signature that identifies the file as a GIF
	// file.  This will consist of one of the two 6-byte strings "GIF87a"
	// or "GIF89a", so an extra entry is required to NULL-terminate the
	// data to make it a proper string.
	char signature[7];
	status = readFromFile(signature, 6);
	if (status != GIF_Success)
		return status;
	signature[6] = '\0';
	strlwr(signature);

	// If neither of the valid signatures is present, then this is
	// not a recognized version of a GIF file.
	if (strcmp(signature, "gif87a") && strcmp(signature, "gif89a"))
		return GIF_FileIsNotGIF;


	///////////////////////////////////
	//	Read the screen descriptor.  //
	///////////////////////////////////

	// Read the image property data from the file header.
	GIFScreenDescriptor screen;
	status = readFromFile(&screen, sizeof(screen));
	if (status != GIF_Success)
		return status;

	// Calculate the color resolution of the global color table.
	// This tells the number of 3-byte entries in that table.
	int colorResolution = ((screen.PackedFields >> 4) & 0x07) + 1;
	if (colorResolution < 2)
		colorResolution = 2;

	// Check to see if a global color table is present in the file.
	// If so, then allocate a color table and read the data from
	// the file.  This global table will be used by all images that
	// do not have a local color table defined.
	if (screen.PackedFields & GIFGlobalMapPresent) {
		DWORD bitShift = (screen.PackedFields & 0x07) + 1;
		m_GlobalColorMapEntryCount = 1 << bitShift;
		status = readFromFile(m_GlobalColorMap, 3 * m_GlobalColorMapEntryCount);
		if (status != GIF_Success)
			return status;
	}

	// Record the full dimensions of the images in the file.
	m_ImageWidth  = screen.Width;
	m_ImageHeight = screen.Height;

	return GIF_Success;
}


GIFError CGIFFile::decodeStream(GIFFrameBuffer &frame, void *desc)
{
	BYTE dataBlock[256];
	BYTE dataLength;
	GIFError status;
	GIFImageDescriptor *imageDesc = (GIFImageDescriptor*)desc;

	// get the byte which indicates how many bits to start
	readFromFile(&m_LzwCodeSize, 1);

	CGIFDecodeFromMemory decoder;

	decoder.SetInterlacing(m_IsInterlaced);
	decoder.SetMemory(frame.pBuffer, frame.BufferSize, frame.Pitch);
	decoder.SetBounds(imageDesc->LeftPosition, imageDesc->TopPosition, imageDesc->Width, imageDesc->Height);

	m_Codec.SetOutput(&decoder);

	m_Codec.StartDecoder(m_LzwCodeSize);

	for (;;) {
		status = readBlock(dataBlock, dataLength);
		if (status != GIF_Success)
			return status;
		if (dataLength == 0)
			break;

		m_Codec.DecodeStream(dataBlock, dataLength);
	}

	m_Codec.StopDecoder();

	while (dataLength) {
		status = readBlock(dataBlock, dataLength);
		if (status != GIF_Success)
			return status;
	}

	return GIF_Success;
}


GIFError CGIFFile::readBlock(BYTE *buffer, BYTE &blockSize)
{
	GIFError status = readFromFile(&blockSize, 1);
	if (status != GIF_Success)
		return status;

	if (blockSize)
		status = readFromFile(buffer, blockSize);

	return status;
}


GIFError CGIFFile::encodeStream(GIFFrameBuffer &frame, long top, long left, long width, long height)
{
	GIFError status;

	// store the bytes indicating how many bits to start
	status = writeToFile(&m_LzwCodeSize, sizeof(m_LzwCodeSize));
	if (status != GIF_Success)
		return status;

	if (m_FileOffset >= m_FileSize)
		return GIF_FileTooSmall;

	CGIFEncodeToMemory encoder;
	encoder.SetMemory(m_pFileBuffer + m_FileOffset, (m_FileSize - m_FileOffset));

	if (m_Codec.SetOutput(&encoder) != LZW_Success)
		return GIF_CodecError;

	if (m_Codec.StartEncoder(m_LzwCodeSize) != LZW_Success)
		return GIF_CodecError;

	for (long y = top; y < top+height; ++y) {
		BYTE *pPixel = frame.pBuffer + (frame.Pitch * y) + left;
		if (m_Codec.EncodeStream(pPixel, width) != LZW_Success)
			return GIF_CodecError;
	}

	if (m_Codec.StopEncoder() != LZW_Success)
		return GIF_CodecError;

	m_FileOffset += encoder.DataSize();

	return GIF_Success;
}


GIFError CGIFFile::writeControlExtension(void)
{
	GIFError status;

	BYTE extension = 0x21;
	status = writeToFile(&extension, sizeof(extension));
	if (status != GIF_Success)
		return status;

	BYTE label = 0xF9;
	status = writeToFile(&label, sizeof(label));
	if (status != GIF_Success)
		return status;

	GIFControlExtension	graphic;
	graphic.BlockSize				= 4;
	graphic.PackedFields			= 0;
	graphic.DelayTime				= m_DelayTime;
	graphic.TransparentColorIndex	= 0;
	graphic.BlockTerminator			= 0;

	return writeToFile(&graphic, sizeof(graphic));
}


DWORD CGIFFile::highestBit(DWORD value)
{
	DWORD highest = 0;

	while (value != 0) {
		value >>= 1;
		++highest;
	}

	return highest;
}

