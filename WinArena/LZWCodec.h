#if !defined(APP_LZWCodec_h)
#define APP_LZWCodec_h


#include <windows.h>


enum LZWError {
	LZW_Success,
	LZW_DecoderAlreadyRunning,
	LZW_EncoderAlreadyRunning,
	LZW_DecoderNotStarted,
	LZW_EncoderNotStarted,
	LZW_NoOutputInterface,
	LZW_NextCodeOutOfRange,
	LZW_FirstCodeOutOfRange,
	LZW_SuffixCodeOutOfRange,
	LZW_OutputError
};


class CLZWOutput {
public:
	virtual ~CLZWOutput(void);
	virtual LZWError Initialize(void) = 0;
	virtual LZWError WriteBuffer(const BYTE *buffer, const DWORD byteCount) = 0;
	virtual LZWError CleanUp(void) = 0;
};


class CLZWCodec
{
public:
	CLZWCodec(void);
	~CLZWCodec(void);

	LZWError SetOutput(CLZWOutput *pInterface);

	LZWError StartEncoder(DWORD bitCount);
	LZWError StopEncoder(void);
	LZWError EncodeStream(BYTE *streamPtr, DWORD streamLen);
	LZWError EncodeByte(BYTE code);

	LZWError StartDecoder(DWORD bitCount);
	LZWError StopDecoder(void);
	LZWError DecodeStream(BYTE *streamPtr, DWORD streamLen);

private:
	LZWError	writeCode(DWORD code);

	CLZWOutput*	m_pOutputInterface;

	BYTE		m_Buffer[8192];
	DWORD		m_BufferSize;
	DWORD		m_BytesInBuffer;
	bool		m_IsEncoding;
	bool		m_IsDecoding;

	DWORD		m_InitialBitCount;
	DWORD		m_CurrentBitCount;
	DWORD		m_ClearCode;
	DWORD		m_EndCode;
	DWORD		m_NextFreeCode;
	DWORD		m_NextBitShift;
	DWORD		m_BitData;
	DWORD		m_BitsLoaded;
	DWORD		m_PrefixCode;
	DWORD		m_OldCode;
	DWORD		m_FirstCode;

	DWORD		m_PrefixList[4096];
	DWORD		m_SuffixList[4096];

	DWORD*		m_PrefixFirst;
	DWORD*		m_PrefixNext;
	BYTE*		m_CodeList;
};


#endif // APP_LZWCodec_h
