/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MidiSong.cpp 1     12/31/06 11:28a Lee $
//
//	File: MidiSong.cpp
//
/////////////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>


#include "MidiSong.h"


/////////////////////////////////////////////////////////////////////////////
//
CwaMidiSong::CwaMidiSong(void)
	:	m_pBuffer(NULL),
		m_BufferSize(0),
		m_OwnsBuffer(false),
		m_Format(MidiFormat_Unknown)
{
}


/////////////////////////////////////////////////////////////////////////////
//
CwaMidiSong::~CwaMidiSong(void)
{
	FreeMemory();
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiSong::FreeMemory(void)
{
	if ((NULL != m_pBuffer) && m_OwnsBuffer) {
		delete [] m_pBuffer;
	}

	m_pBuffer    = NULL;
	m_BufferSize = 0;
	m_OwnsBuffer = false;
	m_Format     = MidiFormat_Unknown;
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiSong::SetBuffer(BYTE *pData, DWORD byteCount, bool takeOwnership)
{
	FreeMemory();

	m_pBuffer    = pData;
	m_BufferSize = byteCount;
	m_OwnsBuffer = takeOwnership;

	return ScanBuffer();
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiSong::LoadFromFile(wchar_t filename[])
{
	FreeMemory();

	FILE *pFile = _wfopen(filename, L"rb");

	if (NULL == pFile) {
		return false;
	}

	fseek(pFile, 0, SEEK_END);
	DWORD fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	m_pBuffer    = new BYTE[fileSize];
	m_BufferSize = fileSize;
	m_OwnsBuffer = true;

	DWORD readSize = DWORD(fread(m_pBuffer, 1, fileSize, pFile));

	fclose(pFile);

	if (fileSize != readSize) {
		FreeMemory();
		return false;
	}

	return ScanBuffer();
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiSong::ScanBuffer(void)
{
	if ((NULL == m_pBuffer) || (m_BufferSize < 16)) {
		return false;
	}

	DWORD startCode = reinterpret_cast<DWORD*>(m_pBuffer)[0];

	if (FourCC_FORM == startCode) {
		m_EventCount = 0;
		return ScanXmidiData(m_pBuffer, m_BufferSize, 0, 0);
	}
	else if (FourCC_MThd == startCode) {
		return ScanMidi();
	}

	m_Format = MidiFormat_Unknown;

	return false;
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiSong::ScanMidi(void)
{
	return false;
}


void Indent(DWORD level)
{
	for (DWORD i = 0; i < level; ++i) { printf("    "); }
}



/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiSong::ScanXmidiData(BYTE *pBuffer, DWORD bufferSize, DWORD baseOffset, DWORD level)
{
	DWORD offset = 0;
	DWORD chunkSize;

	DWORD type[2];
	type[1] = 0;

	while (offset < bufferSize) {
		DWORD chunkID = *reinterpret_cast<DWORD*>(pBuffer + offset);

		char foo[5];
		*((DWORD*)foo) = chunkID;
		foo[4] = 0;

		offset += 4;

		switch (chunkID) {
			case FourCC_FORM:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				type[0] = *reinterpret_cast<DWORD*>(pBuffer + offset);
				if (type[0] == FourCC_XDIR) {
				}
				else if (type[0] == FourCC_XMID) {
				}
				else {
					printf("FORM did not see XDIR or XMID tag: %s\n", (char*)type);
					return false;
				}

				Indent(level);
				printf("FORM chunk [%s]: %d bytes\n", (char*)type, chunkSize);

				if (false == ScanXmidiData(pBuffer + offset + 4, chunkSize - 4, baseOffset + offset + 4, level + 1)) {
					return false;
				}

				offset += chunkSize;
				break;

			case FourCC_EVNT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				Indent(level);
				printf("EVNT chunk: %d bytes\n", chunkSize);

				if (m_EventCount < ArraySize(m_EventOffsets)) {
					m_EventOffsets[m_EventCount] = baseOffset + offset;
					m_EventSizes[m_EventCount]   = chunkSize;

					++m_EventCount;
				}
//				ParseTrack(pBuffer + offset, chunkSize);

//				PlayXmidi(pBuffer + offset, chunkSize);

				offset += chunkSize;
				break;

			case FourCC_INFO:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				Indent(level);
				printf("INFO chunk: %d bytes\n", chunkSize);

				Indent(level+1);
//				HexDump(pBuffer+offset, chunkSize);

				if (chunkSize >= 2) {
//					m_TrackCount = SwizzleWORD(pBuffer + offset);
					printf("track count = %d\n", DWORD(pBuffer[offset]));
						//SwizzleWORD(pBuffer + offset));
				}

				offset += chunkSize;
				break;

			case FourCC_CAT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				type[0] = *reinterpret_cast<DWORD*>(pBuffer + offset);
				if (type[0] == FourCC_XMID) {
				}
				else {
					printf("FORM did not see XDIR or XMID tag: %s\n", (char*)type);
					return false;
				}

				Indent(level);
				printf("CAT chunk: %d bytes\n", chunkSize);

				if (false == ScanXmidiData(pBuffer + offset + 4, chunkSize - 4, baseOffset + offset + 4, level + 1)) {
					return false;
				}

				offset += chunkSize;
				break;

			case FourCC_RBRN:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				Indent(level);
				printf("RBRN chunk: %d bytes\n", chunkSize);

				offset += chunkSize;
				break;

			case FourCC_TIMB:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				Indent(level);
				printf("TIMB chunk: %d bytes\n", chunkSize);

				Indent(level+1);
//				HexDump(pBuffer+offset, chunkSize);

				offset += chunkSize;
				break;

			default:
				printf("Unknown chunk type: 0x%08X\n", chunkID);
				return false;
		}
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiSong::ExportXmidi(char filename[], DWORD volume)
{
	FILE *pFile = fopen(filename, "wb");
	if (NULL == pFile) {
		return;
	}

	fwrite("MThd", 4, 1, pFile);

	DWORD headerSize = SwizzleDWORD(DWORD(6));
	WORD  format     = SwizzleWORD(WORD(0));
	WORD  trackCount = SwizzleWORD(WORD(1));
	WORD  deltaTime  = SwizzleWORD(WORD(96));

	fwrite(&headerSize, 4, 1, pFile);
	fwrite(&format, 2, 1, pFile);
	fwrite(&trackCount, 2, 1, pFile);
	fwrite(&deltaTime, 2, 1, pFile);

	fwrite("MTrk", 4, 1, pFile);

	DWORD trackSizeOffset = ftell(pFile);

	DWORD trackSize = 0;
	fwrite(&trackSize, 4, 1, pFile);

	DWORD tempo = 750000;

	BYTE *pTempoPlusTimeSig = reinterpret_cast<BYTE*>(alloca(7+8));
	pTempoPlusTimeSig[ 0] = BYTE(0x00);
	pTempoPlusTimeSig[ 1] = BYTE(0xFF);
	pTempoPlusTimeSig[ 2] = BYTE(0x51);
	pTempoPlusTimeSig[ 3] = BYTE(0x03);
	pTempoPlusTimeSig[ 4] = BYTE(tempo >> 16);
	pTempoPlusTimeSig[ 5] = BYTE(tempo >> 8);
	pTempoPlusTimeSig[ 6] = BYTE(tempo);
	pTempoPlusTimeSig[ 7] = BYTE(0x00);
	pTempoPlusTimeSig[ 8] = BYTE(0xFF);
	pTempoPlusTimeSig[ 9] = BYTE(0x58);
	pTempoPlusTimeSig[10] = BYTE(0x04);
	pTempoPlusTimeSig[11] = BYTE(0x04);
	pTempoPlusTimeSig[12] = BYTE(0x02);
	pTempoPlusTimeSig[13] = BYTE(0x18);
	pTempoPlusTimeSig[14] = BYTE(0x08);

	BYTE endBits[4];
	endBits[0] = BYTE(0x00);
	endBits[1] = BYTE(0xFF);
	endBits[2] = BYTE(0x2F);
	endBits[3] = BYTE(0x00);

	bool hasAddedSigs = false;

	DWORD offsetBegin = ftell(pFile);
	ExportRecurse(pFile, &pTempoPlusTimeSig, m_pBuffer, m_BufferSize, volume);
	DWORD offsetEnd = ftell(pFile);

	fwrite(endBits, 1, 4, pFile);
	trackSize += 4;

	trackSize += offsetEnd - offsetBegin;

	trackSize = SwizzleDWORD(trackSize);
	fseek(pFile, trackSizeOffset, SEEK_SET);
	fwrite(&trackSize, 4, 1, pFile);

	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiSong::ExportRecurse(FILE *pFile, BYTE **ppTempoPlusTimeSig, BYTE *pBuffer, DWORD bufferSize, DWORD volume)
{
	DWORD offset    = 0;
	DWORD chunkSize = 0;

	char strg[5];
	strg[4] = 0;

	while (offset < bufferSize) {
		DWORD chunkID = *reinterpret_cast<DWORD*>(pBuffer + offset);

		offset += 4;

		reinterpret_cast<DWORD*>(strg)[0] = chunkID;
		LogMessage("chunk %s %d", strg, bufferSize);

		switch (chunkID) {
			case FourCC_FORM:
			case FourCC_CAT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				ExportRecurse(pFile, ppTempoPlusTimeSig, pBuffer + offset + 4, chunkSize - 4, volume);

				offset += chunkSize;
				break;

			case FourCC_EVNT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4;

				ExportNotes(pFile, ppTempoPlusTimeSig, pBuffer + offset, chunkSize, volume);

				offset += chunkSize;
				break;

			default:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset   += 4 + chunkSize;
				break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ExportNotes()
//
void CwaMidiSong::ExportNotes(FILE *pFile, BYTE **ppTempoPlusTimeSig, BYTE *pBuffer, DWORD bufferSize, DWORD volume)
{
	CwaMidiNoteQueue noteQueue;

	DWORD offset        = 0;
	DWORD trackTime     = 0;
	DWORD prevTrackTime = 0;
	bool  firstNoteSeen = true;
	DWORD command       = 0;
	DWORD tempo			= 500000;
	DWORD timeNumer		= 1;
	DWORD timeDenom		= 1;

	DWORD loopOffset	= 0;

	BYTE  lastVolume[3];

	BYTE  eventBuffer[64];

	while (offset < bufferSize) {
		DWORD ticks = 0;

		while (0 == (0x80 & pBuffer[offset])) {
			ticks += DWORD(pBuffer[offset++]);
		}

		ticks = (timeNumer * ticks) / timeDenom;

		if (0 != (0x80 & pBuffer[offset])) {
			command = DWORD(pBuffer[offset++]);
		}

		// Only count time after we've seen the first note.
		// This effectively skips the control/program operations at
		// the start of each event that can cause seconds-long pauses.
		if (firstNoteSeen) {
			trackTime += ticks;
		}

		BYTE* pEvent		= NULL;
		DWORD eventSize		= 0;
		DWORD bits			= 0;
		DWORD noteDuration	= 0;

		if ((command >= 0x80) && (command <= 0xEF)) {
			DWORD a = DWORD(pBuffer[offset]);
			DWORD b = DWORD(pBuffer[offset+1]);

			switch (command & 0xF0) {
				case 0xB0: // Control change
					if (116 == a) {
						// Record the jump address for a loop command.  Looping
						// is ignored for export, but we need to track if the
						// 116 command has been seen if we come across a 117.
						loopOffset = offset + 2;
					}
					else if (117 == a) {
						// Some of the tracks have loop commands before the loop
						// address is set.  Ignore those commands.  Otherwise,
						// any loop command is treated as an "end of track"
						// command, since some tracks have extra music past the
						// end of the track ("oversnow" being the worst, with
						// about 3 extra minutes of jingle bells).
						if (0 != loopOffset) {
							// Set the offset to the end of the buffer, which
							// will break the loop.  The "break" only breaks
							// us out of the switch, so this is needed to end
							// the while loop.
							offset = bufferSize;
							break;
						}
					}

					if (7 == a) {
						lastVolume[0] = BYTE(command);
						lastVolume[1] = BYTE(a);
						lastVolume[2] = BYTE(b);

						b = (b * volume) / 100;
					}

					// fall through to next set of case statements

				case 0x80: // Note Off
				case 0xA0: // Key after-touch
				case 0xE0: // Pitch wheel change
					offset += 2;

					eventBuffer[0] = BYTE(command);
					eventBuffer[1] = BYTE(a);
					eventBuffer[2] = BYTE(b);

					pEvent    = eventBuffer;
					eventSize = 3;
					break;

				case 0xC0: // Program (patch) change
				case 0xD0: // Channel after-touch
					offset += 1;

					eventBuffer[0] = BYTE(command);
					eventBuffer[1] = BYTE(a);

					pEvent    = eventBuffer;
					eventSize = 2;
					break;

				case 0x90: // Note On
					offset += 2;

					bits = DWORD(pBuffer[offset++]);

					noteDuration = bits & 0x7F;

					if (0x80 & bits) {
						bits = DWORD(pBuffer[offset++]);
						noteDuration = (noteDuration << 7) | (bits & 0x7F);
					}

					eventBuffer[0] = BYTE(command);
					eventBuffer[1] = BYTE(a);
					eventBuffer[2] = BYTE(b);

					pEvent    = eventBuffer;
					eventSize = 3;

					noteQueue.InsertNote(trackTime+noteDuration, (0x80) | (command & 0x0F) | (a << 8) | (b << 16));

					firstNoteSeen = true;

					if (NULL != *ppTempoPlusTimeSig) {
						fwrite(*ppTempoPlusTimeSig, 1, 7+8, pFile);
						*ppTempoPlusTimeSig = NULL;
					}

					break;

				default:
					break;
			}
		}
		else if (0xFF == command) {
			DWORD metaCmd   = DWORD(pBuffer[offset++]);
			DWORD byteCount = DWORD(pBuffer[offset++]);

			pEvent    = pBuffer + offset - 3;
			eventSize = byteCount + 3;

			offset += byteCount;

			if (0x51 == metaCmd) {
				tempo = (DWORD(pEvent[3]) << 16) | (DWORD(pEvent[4]) << 8) | DWORD(pEvent[5]);
				pEvent = NULL;
//timeNumer = 3 * 1000;
//timeDenom = tempo / 1000;
			}

			if (0x58 == metaCmd) {
//				timeNumer = 4 * DWORD(pEvent[3]);
//				timeDenom = 1 << DWORD(pEvent[4]);

				memcpy(eventBuffer, pEvent, 7);
				eventBuffer[5] = 4 * eventBuffer[5];

/*
				eventBuffer[0] = BYTE(0xFF);
				eventBuffer[1] = BYTE(0x58);
				eventBuffer[2] = BYTE(4);
				eventBuffer[3] = BYTE(4);
				eventBuffer[4] = BYTE(2);
				eventBuffer[5] = BYTE(96);
				eventBuffer[6] = BYTE(8);
*/
//				pEvent = eventBuffer;
				pEvent = NULL;
			}

			if (0x2F == metaCmd) {
				pEvent = NULL;
				offset = bufferSize;
			}
		}
		else {
			pEvent         = eventBuffer;
			eventBuffer[0] = BYTE(command);
			eventSize      = 1;
		}


		DWORD count = 0;

		for ( ; count < noteQueue.m_QueueDepth; ++count) {
			if (noteQueue.m_Queue[count].Ticks < trackTime) {
				DWORD ticks = noteQueue.m_Queue[count].Ticks - prevTrackTime;
				EncodeAndWriteMidiTicks(pFile, ticks);
				prevTrackTime = noteQueue.m_Queue[count].Ticks;
				fwrite(&(noteQueue.m_Queue[count].Message), 1, 3, pFile);
			}
			else {
				break;
			}
		}

		if (count > 0) {
			noteQueue.PopNotes(count);
		}

		if (NULL != pEvent) {
			DWORD ticks = trackTime - prevTrackTime;
			EncodeAndWriteMidiTicks(pFile, ticks);
			fwrite(pEvent, 1, eventSize, pFile);
			prevTrackTime = trackTime;
		}
	}

	// Flush all remaining off commands.
	for (DWORD count = 0; count < noteQueue.m_QueueDepth; ++count) {
		DWORD ticks = noteQueue.m_Queue[count].Ticks - prevTrackTime;
		EncodeAndWriteMidiTicks(pFile, ticks);
		prevTrackTime = noteQueue.m_Queue[count].Ticks;
		fwrite(&(noteQueue.m_Queue[count].Message), 1, 3, pFile);
	}

	// Write out one final control.  This is a needless volume setting, and is
	// only used to extend the duration of the track to assure that all players
	// will play the full duration of the song before stopping (primarily needed
	// for Media Player).
	EncodeAndWriteMidiTicks(pFile, 100);
	fwrite(lastVolume, 1, 3, pFile);
}




/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaMidiNoteQueue::CwaMidiNoteQueue(void)
	:	m_QueueDepth(0)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	Reset()
//
void CwaMidiNoteQueue::Reset(void)
{
	m_QueueDepth = 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	InsertNote()
//
void CwaMidiNoteQueue::InsertNote(DWORD ticks, DWORD message)
{
	if (m_QueueDepth < ArraySize(m_Queue)) {
		DWORD insertAt = m_QueueDepth;

		for (DWORD i = 0; i < m_QueueDepth; ++i) {
			if (ticks < m_Queue[i].Ticks) {
				insertAt = i;
				break;
			}
		}

		for (DWORD i = m_QueueDepth; i > insertAt; --i) {
			m_Queue[i] = m_Queue[i-1];
		}

		++m_QueueDepth;

		m_Queue[insertAt].Ticks   = ticks;
		m_Queue[insertAt].Message = message;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PopNotes()
//
void CwaMidiNoteQueue::PopNotes(DWORD count)
{
	if (count >= m_QueueDepth) {
		m_QueueDepth = 0;
	}
	else {
		for (DWORD i = count; i < m_QueueDepth; ++i) {
			m_Queue[i-count] = m_Queue[i];
		}

		m_QueueDepth -= count;
	}

char foo[256];
sprintf(foo, "pop %d %d\n", count, m_QueueDepth);
OutputDebugString(foo);
}


void EncodeAndWriteMidiTicks(FILE *pFile, DWORD ticks)
{
//	LogMessage("ticks = %d", ticks);

	DWORD timeWords = 1;
	DWORD timeBits  = ticks & 0x7F;
	while ((ticks >>= 7) > 0) {
		timeBits <<= 8;
		timeBits |= 0x80 | (ticks & 0x7F);
		++timeWords;
	}

	fwrite(&timeBits, 1, timeWords, pFile);
}


