/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MidiPlayer.h 1     12/31/06 11:28a Lee $
//
//	File: MidiPlayer.h
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#include "MidiSong.h"


struct MidiEvent_t
{
	DWORD EventNumber;

	// Set RepeatCount to zero to loop forever.
	DWORD RepeatCount;

	// Set to FALSE to loop over only the repeated portion of the track.
	// Set to TRUE to loop over the entire track, ignoring the repeat offset.
	DWORD FullRepeat;
};


enum MidiQueueCmd_t
{
	MidiQueue_Song,
	MidiQueue_Start,
	MidiQueue_Stop,
	MidiQueue_Pause,
	MidiQueue_Volume,
	MidiQueue_NextEvent,
};


struct MidiQueueData_t
{
	MidiQueueCmd_t	Command;
	CwaMidiSong*	pSong;
	MidiEvent_t*	pEvents;
	DWORD			EventCount;
	DWORD			Value;
};


class CwaMidiPlayer
{
private:
	HMIDIOUT	m_hMidi;
	MIDIHDR		m_MidiHeader;

	DWORD		m_Tempo;

	DWORD		m_Command;

	DWORD*		m_pNoteData;
	DWORD		m_MaxNoteOffset;
	DWORD		m_NoteOffset;
	DWORD		m_TrackCount;

	HANDLE		m_hThreadEvent;
	HANDLE		m_hThread;
	DWORD		m_ThreadID;

	CwaMidiNoteQueue	m_NoteQueue;

	volatile bool		m_IsThreadPlaying;

	MidiQueueData_t		m_Queue[32];
	volatile DWORD		m_QueueIndex;
	volatile DWORD		m_QueueCount;

	CRITICAL_SECTION	m_MidiSection;

	//
	// Thread-private data:
	//

	bool			m_RepeatValid;		// has m_RepeatOffset been set for this event?
	DWORD			m_RepeatOffset;

	DWORD			m_EventLoopCount;	// number of times current event has looped
	DWORD			m_EventIndex;		// current event in m_pEventList
	DWORD			m_TotalEventCount;	// total number of events in m_pEventList
	MidiEvent_t*	m_pEventList;		// list of events to play
	CwaMidiSong*	m_pSong;			// song currently being played

	DWORD			m_TrackTime;		// time at which next event should be played

	bool			m_FirstNoteSeen;	// has the first note been found in the current event?
	DWORD			m_EventOffset;		// byte offset into current event buffer

	BYTE*			m_pEventStart;		// pointer to the current event buffer
	DWORD			m_EventSize;		// byte count for m_pEventStart

public:
	CwaMidiPlayer(void);
	~CwaMidiPlayer(void);

	bool Open(void);

	DWORD PackNote(DWORD command, DWORD ticks, DWORD size, BYTE *pTrack);
	DWORD PackMeta(DWORD ticks, BYTE *pTrack);

	void  ParseMidiFile(char filename[]);
	void  ParseTrack(BYTE *pTrack, DWORD trackLength, char filename[]);

	void  PlayEvents(DWORD notes[], DWORD wordCount);
	void  WriteEventsToMidi(char filename[], DWORD notes[], DWORD wordCount);

	void  PlayXmidi(BYTE xmidi[], DWORD byteCount);

	void  DumpEvents(DWORD notes[], DWORD wordCount);

	void  ParseXmidiFile(char filename[], FILE *pLog);
	void  ParseXmidiData(BYTE *pBuffer, DWORD bufferSize, DWORD level, DWORD &trackNumber, FILE *pLog, FILE *pMidi);
	void  ParseXmidiTrack(BYTE *pTrack, DWORD trackLength, FILE *pLog, FILE *pMidi);

	void  SetTempo(DWORD tempo);

	bool  IsSongFinished(void);

	void  QueueSong(CwaMidiSong *pSong, MidiEvent_t *pEvents, DWORD eventCount, bool startNow);
	void  QueueStart(void);
	void  QueueStop(void);
	void  QueuePause(void);
	void  QueueVolume(DWORD volume);
	void  QueueNextEvent(DWORD id);

	bool  StartThread(void);
	void  StopThread(void);

	static unsigned __stdcall JumpStartThread(void *pContext);
	DWORD WorkerThread(void);

	void  HandleEndOfEvent(void);
};


