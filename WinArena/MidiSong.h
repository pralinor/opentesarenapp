/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MidiSong.h 1     12/31/06 11:28a Lee $
//
//	File: MidiSong.h
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#if !defined(ArraySize)
#define ArraySize(x)			(sizeof(x) / sizeof((x)[0]))
#endif

#if !defined(SafeDelete)
#define SafeDelete(x)			{ if (NULL != (x)) { delete (x); (x) = NULL; } }
#endif

#if !defined(SafeDeleteArray)
#define SafeDeleteArray(x)		{ if (NULL != (x)) { delete [] (x); (x) = NULL; } }
#endif


void LogMessage(const char pattern[], ...);

void EncodeAndWriteMidiTicks(FILE *pFile, DWORD ticks);


struct MidiNote_t
{
	DWORD Ticks;
	DWORD Message;
};


class CwaMidiNoteQueue
{
public:
	MidiNote_t	m_Queue[32];
	DWORD		m_QueueDepth;

	CwaMidiNoteQueue(void);

	void Reset(void);
	void InsertNote(DWORD ticks, DWORD message);
	void PopNotes(DWORD count);

	bool HasSpace(void)		{ return m_QueueDepth < (ArraySize(m_Queue) - 4); }
};


enum MidiFormat_t
{
	MidiFormat_Unknown,
	MidiFormat_Midi,
	MidiFormat_Xmidi,
};


inline WORD SwizzleWORD(BYTE *pData)
{
	return (WORD(pData[0]) << 8)
		 | (WORD(pData[1])     );
}


inline WORD SwizzleWORD(WORD data)
{
	return ((data & 0x00FF) << 8)
		 | ((data & 0xFF00) >> 8);
}


inline DWORD SwizzleDWORD(BYTE *pData)
{
	return (DWORD(pData[0]) << 24)
		 | (DWORD(pData[1]) << 16)
		 | (DWORD(pData[2]) <<  8)
		 | (DWORD(pData[3])      );
}


inline DWORD SwizzleDWORD(DWORD data)
{
	return ((data & 0x000000FF) << 24)
		 | ((data & 0x0000FF00) <<  8)
		 | ((data & 0x00FF0000) >>  8)
		 | ((data & 0xFF000000) >> 24);
}

#pragma pack(push)
#pragma pack(1)

struct MidiHeader_t
{
	DWORD Thud;			// "MThd", 0x6468544D
	DWORD Size;			// == 6
	WORD  Format;		// 0 == single track, 1 == synchronous multitrack, 2 == async multitrack
	WORD  TrackCount;	// number of tracks in file
	WORD  DeltaTime;	// ticks per quarter note
};


struct MidiTrack_t
{
	DWORD ID;			// "MTrk", 0x6B72544D
};

#pragma pack(pop)


#define FourCC_CAT		MAKEFOURCC('C','A','T',' ')
#define FourCC_EVNT		MAKEFOURCC('E','V','N','T')
#define FourCC_FORM		MAKEFOURCC('F','O','R','M')
#define FourCC_INFO		MAKEFOURCC('I','N','F','O')
#define FourCC_MThd		MAKEFOURCC('M','T','h','d') // 0x6468544D
#define FourCC_MTrk		MAKEFOURCC('M','T','r','k') // 0x6B72544D
#define FourCC_RBRN		MAKEFOURCC('R','B','R','N')
#define FourCC_TIMB		MAKEFOURCC('T','I','M','B')
#define FourCC_XDIR		MAKEFOURCC('X','D','I','R')
#define FourCC_XMID		MAKEFOURCC('X','M','I','D')
#define FourCC_RBRN		MAKEFOURCC('R','B','R','N')


class CwaMidiSong
{
private:
	BYTE*			m_pBuffer;
	DWORD			m_BufferSize;
	bool			m_OwnsBuffer;
	MidiFormat_t	m_Format;

	DWORD			m_EventCount;
	DWORD			m_EventOffsets[128];
	DWORD			m_EventSizes[128];

	void ExportRecurse(FILE *pFile, BYTE **ppTempoPlusTimeSig, BYTE *pBuffer, DWORD bufferSize, DWORD volume);
	void ExportNotes(FILE *pFile, BYTE **ppTempoPlusTimeSig, BYTE *pBuffer, DWORD bufferSize, DWORD volume);

public:
	CwaMidiSong(void);
	~CwaMidiSong(void);

	void FreeMemory(void);

	bool SetBuffer(BYTE *pData, DWORD byteCount, bool takeOwnership);

	bool LoadFromFile(wchar_t filename[]);

	bool ScanBuffer(void);
	bool ScanMidi(void);
	bool ScanXmidiData(BYTE *pBuffer, DWORD bufferSize, DWORD baseOffset, DWORD level);

	void ExportXmidi(char filename[], DWORD volume);

	DWORD GetEventCount(void)		{ return m_EventCount; }

	BYTE* GetEvent(DWORD id, DWORD &size)
	{
		if (id < m_EventCount) {
			size = m_EventSizes[id];
			LogMessage("GetEvent(%d) = %d, %d", id, m_EventOffsets[id], size);
			return m_pBuffer + m_EventOffsets[id];
		}

		size = 0;

		return NULL;
	}
};


