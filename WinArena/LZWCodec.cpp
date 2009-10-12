/***************************************************************************
*
*	Class:		CLZWCodec
*	File:		CLZWCodec.cpp
*	Author:		JLS
*	Date:		December, 1997
*	Version:	1.0
*
*	This class performs variable-length-code LZW compression.  An instance
*	of this class is able to both compress and decompress a LZW code stream
*	(but only one at a time).
*
*	The efficiency of this mode of compression depends upon several factors,
*	including the initial code size (bits per code) and the entropy of the
*	data being compressed.  Compressing purely random 8-bit data will actually
*	_expand_ the data size by upwards of 35%, whereas compressing a constant
*	stream of values (all zeroes, for instance), can reduce up to 4000 bytes
*	to a single code (but requires millions of repititions of the same value
*	before the compression ratio grows this large).  Because of the wide
*	variability of the compression ratio, it is inadvisable to decompress
*	more than a few codes at a time without risking overflowing the internal
*	data buffer (this can be assumed to be a rare case if it can be assured
*	that compression ratio never grows this large, or that only relatively
*	small amounts of data (under one meg) will be compressed using this
*	codec).
*
*	Utility functions:
*		BytesInBuffer()
*			Returns number of bytes of data currently stored in the internal
*			buffer.
*		GetBufferSize()
*			Returns the maximum number of bytes that can be	stored in the
*			internal buffer.
*		SetBufferSize()
*			Changes the size of the internal buffer.  The size will default
*			to 8K, and it is recommended that no size smaller than this be
*			used.  Note that it is only possible to change the buffer size
*			while the codec is not running.  Once started to either encode
*			or decode, attempting to change the buffer size will fail.
*		ReadFromBuffer()
*			Copies data from the internal buffer to a user-supplied buffer,
*			returning the number of bytes of data which were copied.
*			This internal buffer will contain the compressed output of the
*			encoder, or the decompressed output of the decoder, depending
*			upon which operation the codec is currently performing.
*
*	Encoder:
*		StartEncoder()
*			Takes the number of bits-per-byte of data being encoded, then
*			initializes the codec to encode bytes of data.  The encoder
*			cannot be started if the codec is already running (either to
*			encode or decode).
*		EncodeByte()
*			This function allows the encoder to compress data at the rate
*			of one byte at a time.  As encoded data is accumulated, it is
*			written to the internal buffer one byte at a time.
*		StopEncoder()
*			Terminates the encoding of data, flushing the last of the
*			compressed data to the internal buffer.  Once stopped, the
*			codec may be restarted to encode or decode another data stream.
*
*	Decoder:
*		StartDecoder()
*			Takes the number of bits-per-byte of data contained in the
*			data stream to be decoded.  This value must match that used
*			to encode the data, otherwise decoding will fail.  The decoder
*			cannot be started if the codec is already running (either to
*			encode or decode).
*		DecodeStream()
*			Takes a pointer to a buffer containing a given number of bytes
*			of encoded data.  As the data is decoded, it is written to the
*			internal buffer one byte at a time.
*		StopDecoder()
*			Finishes the decoding process, cleaning up the codec so that it
*			may be reused to either encode or decode data.
*
***************************************************************************/

#include "LZWCodec.h"


const DWORD c_MaxLZWCodes = 4095;


CLZWOutput::~CLZWOutput(void)
{
}


/***************************************************************************
***************************************************************************/

CLZWCodec::CLZWCodec(void)
	:	m_BufferSize(8192),
		m_BytesInBuffer(0),
		m_IsEncoding(false),
		m_IsDecoding(false),
		m_PrefixFirst(NULL),
		m_PrefixNext(NULL),
		m_CodeList(NULL),
		m_pOutputInterface(NULL)
{
}


CLZWCodec::~CLZWCodec(void)
{
	if (m_PrefixNext)
		delete [] m_PrefixNext;
	if (m_PrefixFirst)
		delete [] m_PrefixFirst;
	if (m_CodeList)
		delete [] m_CodeList;
}


LZWError CLZWCodec::SetOutput(CLZWOutput *pInterface)
{
	m_pOutputInterface = pInterface;

	return LZW_Success;
}


LZWError CLZWCodec::StartEncoder(DWORD bitCount)
{
	if (m_IsDecoding)
		return LZW_DecoderAlreadyRunning;
	if (!m_pOutputInterface)
		return LZW_NoOutputInterface;

	m_pOutputInterface->Initialize();

	if (m_PrefixFirst == NULL)
		m_PrefixFirst = new DWORD[c_MaxLZWCodes+1];
	if (m_PrefixNext == NULL)
		m_PrefixNext = new DWORD[c_MaxLZWCodes+1];
	if (m_CodeList == NULL)
		m_CodeList = new BYTE[c_MaxLZWCodes+1];

	DWORD index;
	for (index = 0; index <= c_MaxLZWCodes; ++index) {
		m_PrefixFirst[index] = 0xFFFF;
		m_PrefixNext[index]  = 0xFFFF;
	}

	m_IsEncoding		= true;
	m_BytesInBuffer		= 0;

	m_InitialBitCount	= bitCount;
	m_ClearCode			= 1 << m_InitialBitCount;
	m_EndCode			= m_ClearCode + 1;
	m_NextFreeCode		= m_ClearCode + 1;		// sic!

	m_CurrentBitCount	= m_InitialBitCount + 1;
	m_NextBitShift		= 1 << m_CurrentBitCount;

	m_PrefixCode		= 0xFFFF;
	m_BitData			= 0;
	m_BitsLoaded		= 0;

	return writeCode(m_ClearCode);
}


LZWError CLZWCodec::StopEncoder(void)
{
	if (m_IsDecoding)
		return LZW_DecoderAlreadyRunning;
	if (!m_IsEncoding)
		return LZW_EncoderNotStarted;

	LZWError status;

	status = writeCode(m_PrefixCode);
	if (status != LZW_Success)
		return status;

	status = writeCode(m_ClearCode);
	if (status != LZW_Success)
		return status;

	m_CurrentBitCount	= m_InitialBitCount + 1;
	status = writeCode(m_EndCode);
	if (status != LZW_Success)
		return status;

	if (m_BitsLoaded) {
		m_Buffer[m_BytesInBuffer++] = (BYTE)m_BitData;
		m_BitsLoaded = 0;
	}

	// Flush any remaining encoded data from the buffer.
	if (m_BytesInBuffer > 0)
		m_pOutputInterface->WriteBuffer(m_Buffer, m_BytesInBuffer);

	m_IsEncoding = false;

	m_pOutputInterface->CleanUp();

	return LZW_Success;
}


LZWError CLZWCodec::writeCode(DWORD code)
{
	m_BitData |= code << m_BitsLoaded;
	m_BitsLoaded += m_CurrentBitCount;

	while (m_BitsLoaded >= 8) {
		m_Buffer[m_BytesInBuffer++] = (BYTE)(m_BitData & 0xFF);
		if (m_BytesInBuffer >= 8192) {
			LZWError status = m_pOutputInterface->WriteBuffer(m_Buffer, m_BytesInBuffer);
			if (status != LZW_Success)
				return status;

			m_BytesInBuffer = 0;
		}

		m_BitData = m_BitData >> 8;
		m_BitsLoaded -= 8;
	}

	return LZW_Success;
}


LZWError CLZWCodec::EncodeStream(BYTE *streamPtr, DWORD streamLen)
{
	DWORD i;
	for (i = 0; i < streamLen; ++i) {
		LZWError status = EncodeByte(streamPtr[i]);
		if (status != LZW_Success)
			return status;
	}

	return LZW_Success;
}


LZWError CLZWCodec::EncodeByte(BYTE code)
{
	LZWError status;

	if (!m_IsEncoding)
		return LZW_EncoderNotStarted;

	if (m_PrefixCode == 0xFFFF) {
		m_PrefixCode = code;
		return LZW_Success;
	}

	DWORD seeker;
	seeker = m_PrefixFirst[m_PrefixCode];
	while (seeker != 0xFFFF) {

		// found a string already in string table
		if (m_CodeList[seeker] == code) {
			m_PrefixCode = seeker;
			return LZW_Success;
		}
		else
			seeker = m_PrefixNext[seeker];
	}

	DWORD index;

	// string was not in string table
	status = writeCode(m_PrefixCode);
	if (status != LZW_Success)
		return status;

	++m_NextFreeCode;
	m_CodeList[m_NextFreeCode] = code;
	m_PrefixNext[m_NextFreeCode] = m_PrefixFirst[m_PrefixCode];
	m_PrefixFirst[m_PrefixCode] = m_NextFreeCode;
	m_PrefixCode = code;

	if (m_NextFreeCode >= m_NextBitShift) {

		// out of free slots,
		// output a clear code and re-init everything.
		// catch at 4094, so no code 4095 will ever be
		// written; although unlikely it is possible that
		// some decoders would up their size from 4K to 8K,
		// which would mean that their next read would take
		// in 13 bits instead of 12, potentially scrambling
		// the clear code
		if (m_NextFreeCode >= (c_MaxLZWCodes-1)) {
			status = writeCode(m_ClearCode);
			if (status != LZW_Success)
				return status;

			m_NextFreeCode		= m_ClearCode + 1;
			m_CurrentBitCount	= m_InitialBitCount + 1;
			m_NextBitShift		= 1 << m_CurrentBitCount;

			for (index = 0; index <= c_MaxLZWCodes; ++index) {
				m_PrefixFirst[index] = 0xFFFF;
				m_PrefixNext[index] = 0xFFFF;
			}
		}
		else {
			++m_CurrentBitCount;
			m_NextBitShift = 1 << m_CurrentBitCount;

			if (m_NextBitShift >= c_MaxLZWCodes)
				m_NextBitShift = c_MaxLZWCodes - 1;
		}
	}

	return LZW_Success;
}


DWORD c_GifBitMask[13]	= {	0x000,0x001,0x003,0x007,0x00F,
							0x01F,0x03F,0x07F,0x0FF,
							0x1FF,0x3FF,0x7FF,0xFFF };

LZWError CLZWCodec::StartDecoder(DWORD bitCount)
{
	if (m_IsEncoding)
		return LZW_EncoderAlreadyRunning;
	if (!m_pOutputInterface)
		return LZW_NoOutputInterface;

	m_pOutputInterface->Initialize();

	m_IsDecoding		= true;
	m_BytesInBuffer		= 0;

	m_InitialBitCount	= bitCount;
	m_ClearCode			= 1 << m_InitialBitCount;
	m_EndCode			= m_ClearCode + 1;
	m_NextFreeCode		= m_ClearCode + 2;

	m_CurrentBitCount	= m_InitialBitCount + 1;
	m_NextBitShift		= 1 << m_CurrentBitCount;

	m_OldCode			= 0xFFFF;
	m_FirstCode			= 0xFFFF;
	m_BitData			= 0;
	m_BitsLoaded		= 0;

	return LZW_Success;
}


LZWError CLZWCodec::StopDecoder(void)
{
	if (m_IsEncoding)
		return LZW_EncoderAlreadyRunning;
	if (!m_IsDecoding)
		return LZW_DecoderNotStarted;

	if (m_BytesInBuffer > 0)
		m_pOutputInterface->WriteBuffer(m_Buffer, m_BytesInBuffer);

	m_IsDecoding = false;

	return m_pOutputInterface->CleanUp();
}


LZWError CLZWCodec::DecodeStream(BYTE *streamPtr, DWORD streamLen)
{
	BYTE*	bufPtr;
	BYTE*	stackPtr;
	BYTE	codeStack[4096];
	DWORD	bitsPending;
	DWORD	currentCode;
	DWORD	suffixCode;

	if (!m_IsDecoding)
		return LZW_DecoderNotStarted;

	// Do nothing if the given buffer is empty.
	if (streamLen < 1)
		return LZW_Success;

	stackPtr = codeStack;
	bufPtr = streamPtr;
	bitsPending = streamLen * 8;
	while ((m_BitsLoaded + bitsPending) >= m_CurrentBitCount) {
		while (m_BitsLoaded < m_CurrentBitCount) {
			m_BitData |= DWORD(*(bufPtr++)) << m_BitsLoaded;
			m_BitsLoaded += 8;
			bitsPending -= 8;
			--streamLen;
		}

		currentCode = m_BitData & c_GifBitMask[m_CurrentBitCount];
		m_BitData = m_BitData >> m_CurrentBitCount;
		m_BitsLoaded -= m_CurrentBitCount;

		if (currentCode > m_NextFreeCode)
			return LZW_NextCodeOutOfRange;

		if (currentCode == m_EndCode) {
			break;
		}
		else if (currentCode == m_ClearCode) {
			m_NextFreeCode = m_ClearCode + 2;
			m_CurrentBitCount = m_InitialBitCount + 1;
			m_NextBitShift = 1 << m_CurrentBitCount;
			m_OldCode = 0xFFFF;
		}
		else if (m_OldCode == 0xFFFF) {
			m_OldCode = currentCode;
			m_FirstCode = m_OldCode;
			m_Buffer[m_BytesInBuffer++] = BYTE(m_OldCode);
		}
		else {
			suffixCode = currentCode;

			if (suffixCode >= m_NextFreeCode) {
				suffixCode = m_OldCode;
				*(stackPtr++) = (BYTE)m_FirstCode;
				if (m_FirstCode > 255)
					return LZW_FirstCodeOutOfRange;
			}

			while (suffixCode > m_EndCode) {
				*(stackPtr++) = BYTE(m_SuffixList[suffixCode]);
				suffixCode = m_PrefixList[suffixCode];
			}
			*(stackPtr++) = (BYTE)suffixCode;
			if (suffixCode > 255)
				return LZW_SuffixCodeOutOfRange;

			if (m_NextFreeCode < m_NextBitShift) {
				m_FirstCode = suffixCode;
				m_SuffixList[m_NextFreeCode] = (BYTE)suffixCode;
				m_PrefixList[m_NextFreeCode] = m_OldCode;
				m_OldCode = currentCode;
				++m_NextFreeCode;
			}
			if ((m_NextFreeCode >= m_NextBitShift) && (m_CurrentBitCount < 12)) {
				++m_CurrentBitCount;
				m_NextBitShift = 1 << m_CurrentBitCount;
			}

			while (stackPtr > codeStack)
				m_Buffer[m_BytesInBuffer++] = *(--stackPtr);

			// If buffer is more than half full, flush it.
			if (m_BytesInBuffer >= 4096) {
				m_pOutputInterface->WriteBuffer(m_Buffer, m_BytesInBuffer);
				m_BytesInBuffer = 0;
			}
		}
	}

	// stuff remaining bytes into bitData
	while (streamLen > 0) {
		m_BitData |= DWORD(*(bufPtr++)) << m_BitsLoaded;
		m_BitsLoaded += 8;
		--streamLen;
	}

	return LZW_Success;
}

