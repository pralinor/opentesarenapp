/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/MidiPlayer.cpp 1     12/31/06 11:28a Lee $
//
//	File: MidiPlayer.cpp
//
/////////////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <process.h>


#include "MidiPlayer.h"


/////////////////////////////////////////////////////////////////////////////
//
void LogMessage(const char pattern[], ...)
{
	char message[1024];

	va_list args;
	va_start(args, pattern);
	int length = _vsnprintf(message, ArraySize(message)-2, pattern, args);
	va_end(args);

	message[length  ] = '\n';
	message[length+1] = '\0';

	OutputDebugStringA(message);
}


/////////////////////////////////////////////////////////////////////////////
//
DWORD PrintCommand(BYTE *pData, char label[])
{
	DWORD count = DWORD(*(pData++));

	char buffer[256];
	memcpy(buffer, pData, count);
	buffer[count] = '\0';

	printf("%s: %s\n", label, buffer);

	return count + 1;
}


/////////////////////////////////////////////////////////////////////////////
//
void CALLBACK MidiOutProc(HMIDIOUT /*hMidi*/, UINT /*message*/, DWORD /*instance*/, DWORD /*param1*/, DWORD /*param2*/)
{
	printf("callback\n");
}


/////////////////////////////////////////////////////////////////////////////
//
CwaMidiPlayer::CwaMidiPlayer(void)
	:	m_Tempo(120),
		m_hThread(NULL),
		m_ThreadID(0),
		m_QueueIndex(0),
		m_QueueCount(0)
{
	m_hThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	InitializeCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
CwaMidiPlayer::~CwaMidiPlayer(void)
{
	CloseHandle(m_hThreadEvent);

	DeleteCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiPlayer::Open(void)
{
//	MMRESULT result;

//	result = midiOutOpen(&m_hMidi, MIDI_MAPPER, UINT_PTR(MidiOutProc), 0, CALLBACK_FUNCTION);

	memset(&m_MidiHeader, 0, sizeof(m_MidiHeader));

//	result = midiOutPrepareHeader(m_hMidi, &m_MidiHeader, sizeof(m_MidiHeader));

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
DWORD CwaMidiPlayer::PackNote(DWORD command, DWORD ticks, DWORD size, BYTE *pTrack)
{
	if ((m_NoteOffset + 10) < m_MaxNoteOffset) {
		DWORD a = DWORD(pTrack[0]);
		DWORD b = DWORD(pTrack[1]);

		m_pNoteData[m_NoteOffset+0] = ticks;
		m_pNoteData[m_NoteOffset+1] = 0;
		m_pNoteData[m_NoteOffset+2] = command | (a << 8);

		if (size > 1) {
			m_pNoteData[m_NoteOffset+2] |= b << 16;
		}

		m_NoteOffset += 3;
	}

	return size;
}


/////////////////////////////////////////////////////////////////////////////
//
DWORD CwaMidiPlayer::PackMeta(DWORD ticks, BYTE *pTrack)
{
	DWORD size = DWORD(pTrack[2]);

	printf("meta: %02X, %d: %02X:%02X:%02X:%02X\n", pTrack[1], pTrack[2], pTrack[3], pTrack[4], pTrack[5], pTrack[6]);

	if ((m_NoteOffset + 10) < m_MaxNoteOffset) {
		DWORD byteCount = size + 2;

		if (0x51 == pTrack[1]) {
/*
			BYTE t = pTrack[3];
			pTrack[3] = pTrack[5];
			pTrack[5] = t;
*/
			DWORD tempo = pTrack[5] | (DWORD(pTrack[4]) << 8) | (DWORD(pTrack[3]) <<16);

			printf("tempo: %02X:%02X:%02X = %06X = %d\n", pTrack[3], pTrack[4], pTrack[5], tempo, tempo);
		}

		m_pNoteData[m_NoteOffset+0] = ticks;
		m_pNoteData[m_NoteOffset+1] = 0;
		m_pNoteData[m_NoteOffset+2] = byteCount | MEVT_F_LONG;
		BYTE *p = reinterpret_cast<BYTE*>(m_pNoteData + m_NoteOffset + 3);

		DWORD wordCount = (byteCount + 3) / 4;
		DWORD padded    = wordCount * 4;

		for (DWORD i = 0; i < byteCount; ++i) {
            p[i] = pTrack[i+1];
		}
		for (DWORD i = size; i < padded; ++i) {
			p[i] = 0;
		}

		m_NoteOffset += 3 + wordCount;
	}

	return size + 1;
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::ParseMidiFile(char filename[])
{
	FILE *pFile = fopen(filename, "rb");
	if (NULL == pFile) {
		return;
	}

	fseek(pFile, 0, SEEK_END);
	DWORD size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	BYTE *pBuffer = new BYTE[size];

	fread(pBuffer, size, 1, pFile);

	fclose(pFile);

	if (0x6468544D != *reinterpret_cast<DWORD*>(pBuffer)) {
		printf("No MThd\n");
	}

	char logname[256];
	sprintf(logname, "%s.txt", filename);

	FILE *pLog = fopen(logname, "w");

	m_MaxNoteOffset = 10000;
	m_NoteOffset	= 0;
	m_pNoteData		= new DWORD[m_MaxNoteOffset];

	DWORD offset = 8;

	DWORD format      = SwizzleWORD(pBuffer + offset + 0);
	DWORD trackCount  = SwizzleWORD(pBuffer + offset + 2);
	DWORD deltaTime   = SwizzleWORD(pBuffer + offset + 4);
	DWORD tempo       = 500000;
	DWORD currentTime = 0;

	offset += 6;

	fprintf(pLog, "Format:      %d\n", format);
	fprintf(pLog, "Track Count: %d\n", trackCount);
	fprintf(pLog, "Delta Time:  %d\n", deltaTime);

	while (offset < size) {
		if (0x6B72544D == *reinterpret_cast<DWORD*>(pBuffer + offset)) {
			fprintf(pLog, "Track: offset = %d\n", offset);

			m_NoteOffset = 0;

			offset += 4;
			DWORD trackLength = SwizzleDWORD(pBuffer + offset);
			offset += 4;

			fprintf(pLog, "  length = %d\n", trackLength);

			BYTE *pTrack = pBuffer + offset;
			DWORD trackOffset = 0;

			offset += trackLength;

			HMIDISTRM hStream = 0;
			UINT deviceID = 0;

			MMRESULT result;
			
			DWORD m_Command = 0;

			while (trackOffset < trackLength) {
//				fprintf(pLog, "  %-4d: ", trackOffset);

				DWORD ticks = 0;
				BYTE  bits  = 0;
				do {
					bits = pTrack[trackOffset++];
					ticks = (ticks << 7) | (bits & 0x7F);
//					fprintf(pLog, "     %d\n", bits);
				} while (0 != (0x80 & bits));

				if (0 != (0x80 & pTrack[trackOffset])) {
					m_Command = DWORD(pTrack[trackOffset++]);
				}

				currentTime += ticks;

				if ((m_Command >= 0x80) && (m_Command <= 0xEF)) {
					switch (m_Command & 0xF0) {

						// Note Off
						case 0x80:
							fprintf(pLog, "%6d  %4d off %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, pTrack[trackOffset], pTrack[trackOffset+1]);

							trackOffset += PackNote(m_Command, ticks, 2, pTrack+trackOffset);
							break;

						// Note On
						case 0x90:
							fprintf(pLog, "%6d  %4d on  %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, pTrack[trackOffset], pTrack[trackOffset+1]);

							trackOffset += PackNote(m_Command, ticks, 2, pTrack+trackOffset);
							break;

						// Key after-touch
						case 0xA0:
							fprintf(pLog, "%6d  %4d key %d\n", currentTime, ticks, m_Command & 0xF);

							trackOffset += 2;
							break;

						// Control change
						case 0xB0:
							fprintf(pLog, "%6d  %4d control %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, pTrack[trackOffset], pTrack[trackOffset+1]);

							trackOffset += PackNote(m_Command, ticks, 2, pTrack+trackOffset);
							break;

						// Program (patch) change
						case 0xC0:
							fprintf(pLog, "%6d  %4d program %3d, %3d\n", currentTime, ticks, m_Command & 0xF, pTrack[trackOffset]);

							trackOffset += PackNote(m_Command, ticks, 1, pTrack+trackOffset);
							break;

						// Channel after-touch
						case 0xD0:
							fprintf(pLog, "%6d  %4d channel %d\n", currentTime, ticks, m_Command & 0xF);

							trackOffset += PackNote(m_Command, ticks, 1, pTrack+trackOffset);
							break;

						// Pitch wheel change
						case 0xE0:
							fprintf(pLog, "%6d  %4d pitch %d\n", currentTime, ticks, m_Command & 0xF);

							trackOffset += PackNote(m_Command, ticks, 2, pTrack+trackOffset);
							break;
					}
				}
				else if (0xFF == m_Command) {
					DWORD meta = DWORD(pTrack[trackOffset++]);

					switch (meta) {
						case 0x00:	// Sets the track's sequence number
							fprintf(pLog, "Sequence number: %d bytes\n", DWORD(pTrack[trackOffset]));

							trackOffset += PackMeta(ticks, pTrack + trackOffset - 2);
							break;

						case 0x01:	// Text event
							trackOffset += PrintCommand(pTrack + trackOffset, "Comment:");
							break;

						case 0x02:
							trackOffset += PrintCommand(pTrack + trackOffset, "Copyright:");
							break;

						case 0x03:
							trackOffset += PrintCommand(pTrack + trackOffset, "Track Name:");
							break;

						case 0x04:
							trackOffset += PrintCommand(pTrack + trackOffset, "Instrument Name:");
							break;

						case 0x05:
							trackOffset += PrintCommand(pTrack + trackOffset, "Lyric:");
							break;

						case 0x06:
							trackOffset += PrintCommand(pTrack + trackOffset, "Marker:");
							break;

						case 0x07:
							trackOffset += PrintCommand(pTrack + trackOffset, "Cue Point:");
							break;

						case 0x2F:	// end of track
							fprintf(pLog, "End of track\n");
							trackOffset += 1;
							break;

						case 0x51:	// Set tempo
							tempo = pTrack[trackOffset+3] | (DWORD(pTrack[trackOffset+2]) << 8) | (DWORD(pTrack[trackOffset+1]) <<16);

							fprintf(pLog, "set tempo: %d bytes, %d\n", DWORD(pTrack[trackOffset]), tempo);

					//		trackOffset += PackMeta(ticks, pTrack + trackOffset - 2);
							trackOffset += 4;
							break;

						case 0x54:	// SMPTE offset
							fprintf(pLog, "SMPTE offset: %d bytes\n", DWORD(pTrack[trackOffset]));
							trackOffset += 6;
							break;

						case 0x58:	// Time Signature
							fprintf(pLog, "time signature: %d bytes, 0x%08X\n", DWORD(pTrack[trackOffset]),
								SwizzleDWORD(*((DWORD*)(pTrack+trackOffset+1))));

							trackOffset += PackMeta(ticks, pTrack + trackOffset - 2);
						//	trackOffset += 5;
							break;

						case 0x59:	// Key signature
							fprintf(pLog, "key signature\n");
							trackOffset += 3;
							break;

						case 0x7F:	// Sequencer specific information
							fprintf(pLog, "sequence data: %d bytes\n", DWORD(pTrack[trackOffset]));

					//		trackOffset += PackMeta(ticks, pTrack + trackOffset - 2);
							trackOffset += 1 + DWORD(pTrack[trackOffset]);
							break;

						default:
							fprintf(pLog, "unknown meta 0x%02X\n", meta);
							break;
					}
				}
				else {
					switch (m_Command) {
						case 0xF8:
							fprintf(pLog, "timing clock\n");
							break;

						case 0xFA:
							fprintf(pLog, "start current sequence\n");
							break;

						case 0xFB:
							fprintf(pLog, "continue stopped sequence\n");
							break;

						case 0xFC:
							fprintf(pLog, "stop sequence\n");
							break;

						case 0xFF:
							fprintf(pLog, "NOP\n");
							break;

						default:
							fprintf(pLog, "unknown command 0x%02X, 0x%02X\n", m_Command, ticks);
							break;
					}
				}
			}
/*
			for (DWORD q= 0; q < 400; ++q) {
				char foo[64];
				sprintf(foo, "%08X\n", notes[q]);
				OutputDebugString(foo);
			}
*/
			m_MidiHeader.dwFlags = 0;
m_MidiHeader.lpData = (LPSTR)m_pNoteData;
m_MidiHeader.dwBufferLength = m_MidiHeader.dwBytesRecorded = m_NoteOffset * 4;

			result = midiStreamOpen(&hStream, &deviceID, 1, UINT_PTR(MidiOutProc), 0, CALLBACK_FUNCTION);

			MIDIPROPTIMEDIV prop;
			/* Set the timebase. Here I use 96 PPQN */
			prop.cbStruct  = sizeof(MIDIPROPTIMEDIV);
			prop.dwTimeDiv = deltaTime;
			result = midiStreamProperty(hStream, (LPBYTE)&prop, MIDIPROP_SET|MIDIPROP_TIMEDIV);

			MIDIPROPTEMPO  tempoProp;
			tempoProp.cbStruct = sizeof(MIDIPROPTEMPO);
			tempoProp.dwTempo  = tempo;
			result = midiStreamProperty(hStream, (LPBYTE)&tempoProp, MIDIPROP_SET|MIDIPROP_TEMPO);

			result = midiOutPrepareHeader((HMIDIOUT)hStream, &m_MidiHeader, sizeof(m_MidiHeader));

			result = midiStreamOut(hStream, &m_MidiHeader, sizeof(m_MidiHeader));
			result = midiStreamRestart(hStream);
			result = midiOutSetVolume((HMIDIOUT)hStream, 0x80008000);
			getch();
			result = midiOutUnprepareHeader((HMIDIOUT)hStream, &m_MidiHeader, sizeof(m_MidiHeader));
			result = midiStreamClose(hStream);
		}
		else {
			break;
		}
	}

	delete [] pBuffer;

	fclose(pLog);
}


/////////////////////////////////////////////////////////////////////////////
//
struct SortEntry_t
{
	DWORD Time;
	DWORD Duration;
	DWORD Size;
	BYTE  Data[12];
};

void CwaMidiPlayer::ParseTrack(BYTE *pTrack, DWORD trackLength, char filename[])
{
	DWORD m_Command = 0;
	DWORD trackOffset = 0;


//	HMIDISTRM hStream = 0;
//	UINT deviceID = 0;

//	DWORD tempo = 500000;

	DWORD noteCount = 0;
	const DWORD maxEntries = 10000;
	SortEntry_t entries[maxEntries];

	DWORD currentTime = 0;

	DWORD ticks = 0;

	FILE *pLog = fopen("dump.txt", "w");

	while (trackOffset < trackLength) {
		ticks = 0;

		fprintf(pLog, "     %02X:%02X:%02X:%02X:%02X:%02X\n",
			pTrack[trackOffset],
			pTrack[trackOffset+1],
			pTrack[trackOffset+2],
			pTrack[trackOffset+3],
			pTrack[trackOffset+4],
			pTrack[trackOffset+5]);

		while (0 == (0x80 & pTrack[trackOffset])) {
			fprintf(pLog, "tick: %d\n", pTrack[trackOffset]);
			ticks += DWORD(pTrack[trackOffset++]);
		}

		ticks *= 2;

		currentTime += ticks;

//		ticks = accumTime - currentTime;
/*
		printf("  %-4d: 0x%02X 0x%02X 0x%02X 0x%02X\n      ", trackOffset,
			DWORD(pTrack[trackOffset+0]),
			DWORD(pTrack[trackOffset+1]),
			DWORD(pTrack[trackOffset+2]),
			DWORD(pTrack[trackOffset+3]));
*/
		if (0 != (0x80 & pTrack[trackOffset])) {
			m_Command = DWORD(pTrack[trackOffset++]);
			fprintf(pLog, "command: %d\n", m_Command);
		}

		if ((m_Command >= 0x80) && (m_Command <= 0xEF)) {
			DWORD a = DWORD(pTrack[trackOffset]);
			DWORD b = DWORD(pTrack[trackOffset+1]);

			switch (m_Command & 0xF0) {

				// Note Off
				case 0x80:
					fprintf(pLog, "%6d  %4d off %d, %d, %d\n", currentTime, ticks, m_Command & 0xF, a, b);

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 3;
						entries[noteCount].Data[0]  = BYTE(m_Command);
						entries[noteCount].Data[1]  = BYTE(a);
						entries[noteCount].Data[2]  = BYTE(b);
						++noteCount;
					}

					trackOffset += 2;
					break;

				// Note On
				case 0x90:
					trackOffset += 2;

					if (noteCount < maxEntries) {
						DWORD channel = m_Command & 0xF;
						DWORD c       = DWORD(pTrack[trackOffset++]);
						DWORD delta   = c & 0x7F;

						if (c & 0x80) {
							c     = DWORD(pTrack[trackOffset++]);
							delta = (delta << 7) | (c & 0x7F);
						}

						delta *= 2;

						DWORD d = 0;//DWORD(pTrack[trackOffset++]);

						fprintf(pLog, "%6d  %4d-%-3d on  %d, %d, %d, %d, %d\n", currentTime, ticks, delta, m_Command & 0xF, a, b, c, d);

						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = delta;
						entries[noteCount].Size     = 3;
						entries[noteCount].Data[0]  = BYTE(0x90 | channel);
						entries[noteCount].Data[1]  = BYTE(a);
						entries[noteCount].Data[2]  = BYTE(b);
						++noteCount;

						entries[noteCount].Time     = currentTime + delta;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 3;
						entries[noteCount].Data[0]  = BYTE(0x80 | channel);
						entries[noteCount].Data[1]  = BYTE(a);
						entries[noteCount].Data[2]  = BYTE(b);
						++noteCount;

//						printf("  %4d on  %d, %d, %d\n", ticks, m_Command & 0xF, a, b);
					}

					break;

				// Key after-touch
				case 0xA0:
					printf("  %4d key %d\n", ticks, m_Command & 0xF);

					trackOffset += 2;
					break;

				// Control change
				case 0xB0:
					fprintf(pLog, "%6d  %4d control %d, %d, %d\n", currentTime, ticks, m_Command & 0xF, pTrack[trackOffset], pTrack[trackOffset+1]);

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 3;
						entries[noteCount].Data[0]  = BYTE(m_Command);
						entries[noteCount].Data[1]  = BYTE(a);
						entries[noteCount].Data[2]  = BYTE(b);
						++noteCount;
					}

					trackOffset += 2;

					if (117 == a) {
//						trackOffset = trackLength;
					}
					break;

				// Program (patch) change
				case 0xC0:
					printf("  %4d program %d, %d\n", ticks, m_Command & 0xF, pTrack[trackOffset]);

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 2;
						entries[noteCount].Data[0]  = BYTE(m_Command);
						entries[noteCount].Data[1]  = BYTE(a);
						++noteCount;
					}

					trackOffset += 1;
					break;

				// Channel after-touch
				case 0xD0:
					printf("  %4d channel %d\n", ticks, m_Command & 0xF);

					trackOffset += 1;
					break;

				// Pitch wheel change
				case 0xE0:
//					printf("  %4d pitch %d\n", ticks, m_Command & 0xF);

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 3;
						entries[noteCount].Data[0]  = BYTE(m_Command);
						entries[noteCount].Data[1]  = BYTE(a);
						entries[noteCount].Data[2]  = BYTE(b);
						++noteCount;
					}

					trackOffset += 2;
					break;
			}
		}
		else if (0xFF == m_Command) {
			DWORD meta = DWORD(pTrack[trackOffset++]);

			switch (meta) {
				case 0x00:	// Sets the track's sequence number
					printf("Sequence number\n");

					trackOffset += 3;
					break;

				case 0x01:	// Text event
					trackOffset += PrintCommand(pTrack + trackOffset, "Comment:");
					break;

				case 0x02:
					trackOffset += PrintCommand(pTrack + trackOffset, "Copyright:");
					break;

				case 0x03:
					trackOffset += PrintCommand(pTrack + trackOffset, "Track Name:");
					break;

				case 0x04:
					trackOffset += PrintCommand(pTrack + trackOffset, "Instrument Name:");
					break;

				case 0x05:
					trackOffset += PrintCommand(pTrack + trackOffset, "Lyric:");
					break;

				case 0x06:
					trackOffset += PrintCommand(pTrack + trackOffset, "Marker:");
					break;

				case 0x07:
					trackOffset += PrintCommand(pTrack + trackOffset, "Cue Point:");
					break;

				case 0x2F:	// end of track
					printf("End of track\n");
					trackOffset += 1;
					break;

				case 0x51:	// Set tempo
//					printf("set tempo\n");

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 5;
						entries[noteCount].Data[0]  = pTrack[trackOffset-1];
						entries[noteCount].Data[1]  = pTrack[trackOffset+0];
						entries[noteCount].Data[2]  = pTrack[trackOffset+1];
						entries[noteCount].Data[3]  = pTrack[trackOffset+2];
						entries[noteCount].Data[4]  = pTrack[trackOffset+3];

						DWORD tempo = DWORD(pTrack[trackOffset+3])
							| (DWORD(pTrack[trackOffset+2]) << 8)
							| (DWORD(pTrack[trackOffset+1]) << 16);

printf("tempo = 0x%08X = %d\n", *((DWORD*)&(entries[noteCount].Data[3])), tempo);
static bool done = false;
if (!done) { done = true;
//						++noteCount;
}
					}

					trackOffset += 4;
					break;

				case 0x54:	// SMPTE offset
//					printf("SMPTE offset\n");

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 7;
						entries[noteCount].Data[0]  = pTrack[trackOffset-1];
						entries[noteCount].Data[1]  = pTrack[trackOffset+0];
						entries[noteCount].Data[2]  = pTrack[trackOffset+1];
						entries[noteCount].Data[3]  = pTrack[trackOffset+2];
						entries[noteCount].Data[4]  = pTrack[trackOffset+3];
						entries[noteCount].Data[5]  = pTrack[trackOffset+4];
						entries[noteCount].Data[6]  = pTrack[trackOffset+5];
						entries[noteCount].Data[7]	= 0;
	//					++noteCount;
					}

					trackOffset += 6;
					break;

				case 0x58:	// Time Signature
//					printf("time signature\n");

					if (noteCount < maxEntries) {
						entries[noteCount].Time     = currentTime;
						entries[noteCount].Duration = 0;
						entries[noteCount].Size     = 6;
						entries[noteCount].Data[0]  = pTrack[trackOffset-1];
						entries[noteCount].Data[1]  = pTrack[trackOffset+0];
						entries[noteCount].Data[2]  = pTrack[trackOffset+1];
						entries[noteCount].Data[3]  = pTrack[trackOffset+2];
						entries[noteCount].Data[4]  = pTrack[trackOffset+3];
						entries[noteCount].Data[5]  = pTrack[trackOffset+4];
						entries[noteCount].Data[6]	= 0;
						entries[noteCount].Data[7]	= 0;
/*
						entries[noteCount].Data[2]  = 0x04;
						entries[noteCount].Data[3]  = 0x02;
						entries[noteCount].Data[4]  = 0x60;
						entries[noteCount].Data[5]  = 0x08;
*/
						BYTE *p = pTrack+trackOffset-1;
printf("meta: %02X, %d: %02X:%02X:%02X:%02X\n", p[0], p[1], p[2], p[3], p[4], p[5]);
printf("timesig = 0x%08X\n", *((DWORD*)&(entries[noteCount].Data[2])));
static int sigcount = 0;
if (++sigcount > 1) {
	++noteCount;
}
					}

					trackOffset += 5;
					break;

				case 0x59:	// Key signature
					printf("key signature\n");
					trackOffset += 3;
					break;

				case 0x7F:	// Sequencer specific information
					printf("sequence data\n");
					trackOffset += 1 + DWORD(pTrack[trackOffset]);
					break;

				default:
					printf("unknown meta 0x%02X\n", meta);
					break;
			}
		}
		else {
			switch (m_Command) {
				case 0xF8:
					printf("timing clock\n");
					break;

				case 0xFA:
					printf("start current sequence\n");
					break;

				case 0xFB:
					printf("continue stopped sequence\n");
					break;

				case 0xFC:
					printf("stop sequence\n");
					break;

				case 0xFF:
					printf("NOP\n");
					break;

				default:
					printf("unknown command 0x%02X, 0x%02X\n", m_Command, ticks);
					break;
			}
		}
	}

	printf("events = %d\n", noteCount);

	if (noteCount > 1) {
		for (DWORD i = noteCount - 1; i > 0; --i) {
			DWORD maxTime  = 0;
			DWORD maxIndex = 0;

			for (DWORD j = 0; j <= i; ++j) {
				if (entries[j].Time >= maxTime) {
					maxTime  = entries[j].Time;
					maxIndex = j;
				}
			}

			if (maxIndex != i) {
				SortEntry_t temp  = entries[maxIndex];
				for (DWORD j = maxIndex; j < i; ++j) {
					entries[j] = entries[j+1];
				}
				entries[i] = temp;
			}
		}
	}

	BYTE notesOn[16][128];
	for (DWORD i = 0; i < 16; ++i) {
		for (DWORD j = 0; j < 128; ++j) {
			notesOn[i][j] = 0;
		}
	}

	DWORD notes[30000];

	DWORD prevTime = 0;
	DWORD base = 0;

	for (DWORD i = 0; i < noteCount; ++i) {
		notes[base+0] = (entries[i].Time - prevTime) * 3;
		notes[base+1] = 0;

		DWORD command = 0;
		DWORD com2    = 0;
		DWORD size    = 3;
		DWORD count	  = entries[i].Size;

		if (entries[i].Size < 4) {
			BYTE *p = reinterpret_cast<BYTE*>(notes + base + 2);

			p[0] = entries[i].Data[0];
			p[1] = entries[i].Data[1];
			p[2] = entries[i].Data[2];
			p[3] = 0;

			if (0x80 == (p[0] & 0xF0)) {
				notesOn[p[0]&0x0F][p[1]&0x7F] = 0;
			}
			if (0x90 == (p[0] & 0xF0)) {
				notesOn[p[0]&0x0F][p[1]&0x7F] = 1;
			}

			command = notes[base+2];
		}
		else {
			notes[base+2] = MEVT_F_LONG | entries[i].Size;

			BYTE *p = reinterpret_cast<BYTE*>(notes + base + 3);

			for (DWORD j = 0; j < count; ++j) {
				p[j] = entries[i].Data[j];
			}
			for (DWORD k = count; k < 12; ++k) {
				p[k] = 0;
			}

			command = notes[base+3];
			com2    = notes[base+4];

			size = 3 + ((count + 3) / 4);
		}

//		printf("%4d: %4d %4d %4d %8X %8X %d %d\n", i, entries[i].Time, notes[base+0], entries[i].Duration, command, com2, count, base);

		prevTime = entries[i].Time;

		base += size;
	}

	for (DWORD i = 0; i < 16; ++i) {
		for (DWORD j = 0; j < 128; ++j) {
			if (notesOn[i][j]) {
				printf(" BAD NOTE [%d][%d]\n", i, j);
				notes[base+0] = (entries[i].Time - prevTime) * 3;
				notes[base+1] = 0;

				BYTE *p = reinterpret_cast<BYTE*>(notes + base + 2);

				p[0] = 0x80 | BYTE(i);
				p[1] = BYTE(j);
				p[2] = 0;
				p[3] = 0;

				base += 3;
			}
		}
	}

	fclose(pLog);

/*
			for (DWORD q= 0; q < 400; ++q) {
				char foo[64];
				sprintf(foo, "%08X\n", notes[q]);
				OutputDebugString(foo);
			}
*/

	WriteEventsToMidi(filename, notes, base);
//	DumpEvents(notes, base);
//	PlayEvents(notes, base);

/*
	MMRESULT result;

	result = midiStreamOpen(&hStream, &deviceID, 1, UINT_PTR(MidiOutProc), 0, CALLBACK_FUNCTION);

	MIDIPROPTIMEDIV prop;
	prop.cbStruct  = sizeof(MIDIPROPTIMEDIV);
	prop.dwTimeDiv = 192;
	result = midiStreamProperty(hStream, (LPBYTE)&prop, MIDIPROP_SET|MIDIPROP_TIMEDIV);
tempo = 250000;
	MIDIPROPTEMPO  propTempo;
	propTempo.cbStruct = sizeof(MIDIPROPTEMPO);
	propTempo.dwTempo  = tempo;
	result = midiStreamProperty(hStream, (LPBYTE)&propTempo, MIDIPROP_SET|MIDIPROP_TEMPO);


	m_MidiHeader.lpData = (char*)pTrack;
	m_MidiHeader.dwBufferLength = trackLength;
	m_MidiHeader.dwBytesRecorded = trackLength;
	m_MidiHeader.dwFlags = 0;


m_MidiHeader.lpData = (LPSTR)notes;
m_MidiHeader.dwBufferLength = m_MidiHeader.dwBytesRecorded = base * 4;

	result = midiOutPrepareHeader((HMIDIOUT)hStream, &m_MidiHeader, sizeof(m_MidiHeader));

	result = midiOutSetVolume((HMIDIOUT)hStream, 0x80008000);
	result = midiStreamOut(hStream, &m_MidiHeader, sizeof(m_MidiHeader));
	result = midiStreamRestart(hStream);
//	Sleep(10000);
//	result = midiOutSetVolume((HMIDIOUT)hStream, 0x80008000);
//	Sleep(20000);
	getch();
	result = midiOutUnprepareHeader((HMIDIOUT)hStream, &m_MidiHeader, sizeof(m_MidiHeader));

	midiStreamClose(hStream);
*/
}


/////////////////////////////////////////////////////////////////////////////
//
//	PlayEvents()
//
void CwaMidiPlayer::PlayEvents(DWORD notes[], DWORD wordCount)
{
	MMRESULT result;

	HMIDISTRM hStream = 0;
	UINT deviceID = 0;

	result = midiStreamOpen(&hStream, &deviceID, 1, UINT_PTR(MidiOutProc), 0, CALLBACK_NULL);

	MIDIPROPTIMEDIV prop;
	prop.cbStruct  = sizeof(MIDIPROPTIMEDIV);
	prop.dwTimeDiv = 384;
	result = midiStreamProperty(hStream, reinterpret_cast<BYTE*>(&prop), MIDIPROP_SET|MIDIPROP_TIMEDIV);

	MIDIPROPTEMPO  propTempo;
	propTempo.cbStruct = sizeof(MIDIPROPTEMPO);
	propTempo.dwTempo  = 500000;
	result = midiStreamProperty(hStream, reinterpret_cast<BYTE*>(&propTempo), MIDIPROP_SET|MIDIPROP_TEMPO);

	result = midiOutSetVolume((HMIDIOUT)hStream, 0x80008000);
	result = midiStreamOut(hStream, &m_MidiHeader, sizeof(m_MidiHeader));

	DWORD baseTime    = timeGetTime();
	DWORD currentTime = 0;
	DWORD offset      = 0;


	while (offset < wordCount) {
		currentTime += notes[offset];

		DWORD size = 3;

		if (notes[offset] != 0) {
			DWORD ticks = ((timeGetTime() - baseTime) * 75) / 100;

			while (ticks < currentTime) {
				Sleep(10);
				ticks = ((timeGetTime() - baseTime) * 75) / 100;
			}
		}

		if (MEVT_F_LONG & notes[offset+2]) {
			size = 3 + (((notes[offset+2] & 0xF) + 3) / 4);
		}
		else {
			printf(" %6d %08X\n", currentTime, notes[offset+2]);
			midiOutShortMsg(HMIDIOUT(hStream), notes[offset+2]);
		}

		offset += size;

		if (offset >= wordCount) {
			printf("loop\n");
			offset = 0;
		}

		if (_kbhit()) {
			getch();
			printf("break\n");
			break;
		}
	}

	midiStreamClose(hStream);
}


/////////////////////////////////////////////////////////////////////////////
//
//	WriteEventsToMidi()
//
void CwaMidiPlayer::WriteEventsToMidi(char filename[], DWORD notes[], DWORD wordCount)
{
	FILE *pFile = fopen(filename, "wb");

	fwrite("MThd", 4, 1, pFile);

	DWORD headerSize = SwizzleDWORD(DWORD(6));
	WORD  format     = SwizzleWORD(WORD(0));
	WORD  trackCount = SwizzleWORD(WORD(1));
	WORD  deltaTime  = SwizzleWORD(WORD(384));

	fwrite(&headerSize, 4, 1, pFile);
	fwrite(&format, 2, 1, pFile);
	fwrite(&trackCount, 2, 1, pFile);
	fwrite(&deltaTime, 2, 1, pFile);

	fwrite("MTrk", 4, 1, pFile);

	DWORD trackSizeOffset = ftell(pFile);

	DWORD trackSize = 0;
	fwrite(&trackSize, 4, 1, pFile);

	DWORD tempo = 500000;

	BYTE tempoBits[7];
	tempoBits[0] = BYTE(0x00);
	tempoBits[1] = BYTE(0xFF);
	tempoBits[2] = BYTE(0x51);
	tempoBits[3] = BYTE(0x03);
	tempoBits[4] = BYTE(tempo >> 16);
	tempoBits[5] = BYTE(tempo >> 8);
	tempoBits[6] = BYTE(tempo);

	BYTE timeSig[8];
	timeSig[0] = BYTE(0x00);
	timeSig[1] = BYTE(0xFF);
	timeSig[2] = BYTE(0x58);
	timeSig[3] = BYTE(0x04);
	timeSig[4] = BYTE(0x06);
	timeSig[5] = BYTE(0x03);
	timeSig[6] = BYTE(0x24);
	timeSig[7] = BYTE(0x08);

	bool addedSigs = false;

	DWORD offset = 0;

	BYTE tag = 0xFF;

	while (offset < wordCount) {

		DWORD ticks     = notes[offset];
		DWORD eventSize = 3;
		DWORD dataCount = 0;

		if ((false == addedSigs) && (0 == (MEVT_F_LONG & notes[offset+2]))) {
			addedSigs = true;

			fwrite(tempoBits, 1, 7, pFile);
			trackSize += 7;

			fwrite(timeSig, 1, 8, pFile);
			trackSize += 8;
		}


		DWORD timeWords = 1;
		DWORD timeBits  = ticks & 0x7F;
		while ((ticks >>= 7) > 0) {
			timeBits <<= 8;
			timeBits |= 0x80 | (ticks & 0x7F);
			++timeWords;
		}

		if (MEVT_F_LONG & notes[offset+2]) {
			dataCount = notes[offset+2] & 0xFF;
			eventSize = 3 + ((dataCount + 3) / 4);

			fwrite(&timeBits, 1, timeWords, pFile);
			fwrite(&tag, 1, 1, pFile);
			fwrite(notes + offset + 3, 1, dataCount, pFile);

			trackSize += timeWords + dataCount + 1;

		}
		else {
			dataCount = 3;

			if ((0xC0 == (notes[offset+2] & 0xF0)) || (0xD0 == (notes[offset+2] & 0xF0))) {
				dataCount = 2;
			}

//if (((0xF0) & notes[offset+2]) < 0x9F) {
			fwrite(&timeBits, 1, timeWords, pFile);
			fwrite(notes + offset + 2, 1, dataCount, pFile);

			trackSize += timeWords + dataCount;
//}
		}

		offset += eventSize;
	}

	BYTE endBits[4];
	endBits[0] = BYTE(0x00);
	endBits[1] = BYTE(0xFF);
	endBits[2] = BYTE(0x2F);
	endBits[3] = BYTE(0x00);

	fwrite(endBits, 1, 4, pFile);
	trackSize += 4;

	trackSize = SwizzleDWORD(trackSize);
	fseek(pFile, trackSizeOffset, SEEK_SET);
	fwrite(&trackSize, 4, 1, pFile);

	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
//	PlayXmidi()
//
void CwaMidiPlayer::PlayXmidi(BYTE xmidi[], DWORD byteCount)
{
	MMRESULT result;

	HMIDISTRM hStream = 0;
	UINT deviceID = 0;

	result = midiStreamOpen(&hStream, &deviceID, 1, UINT_PTR(MidiOutProc), 0, CALLBACK_NULL);

	MIDIPROPTIMEDIV prop;
	prop.cbStruct  = sizeof(MIDIPROPTIMEDIV);
	prop.dwTimeDiv = 384;
	result = midiStreamProperty(hStream, (LPBYTE)&prop, MIDIPROP_SET|MIDIPROP_TIMEDIV);

	MIDIPROPTEMPO  propTempo;
	propTempo.cbStruct = sizeof(MIDIPROPTEMPO);
	propTempo.dwTempo  = 250000;
	result = midiStreamProperty(hStream, (LPBYTE)&propTempo, MIDIPROP_SET|MIDIPROP_TEMPO);

	result = midiOutSetVolume((HMIDIOUT)hStream, 0x80008000);
	result = midiStreamOut(hStream, &m_MidiHeader, sizeof(m_MidiHeader));

	DWORD baseTime  = timeGetTime();
	DWORD trackTime = 0;
	DWORD offset    = 0;

	DWORD noteDuration = 0;
	DWORD bits = 0;

	m_NoteQueue.Reset();

	DWORD command = 0;

	bool loop = true;

	bool  repeatValid  = false;
	DWORD repeatOffset = 0;

	while (loop) {
		while ((offset < byteCount) && m_NoteQueue.HasSpace()) {
			DWORD baseCommandOffset = offset;

			DWORD ticks = 0;

			while (0 == (0x80 & xmidi[offset])) {
//				printf("tick: %d\n", xmidi[offset]);
				ticks += DWORD(xmidi[offset++]);
			}

			trackTime += ticks;

			if (0 != (0x80 & xmidi[offset])) {
				command = DWORD(xmidi[offset++]);
			}

			if ((command >= 0x80) && (command <= 0xEF)) {
				DWORD a = DWORD(xmidi[offset]);
				DWORD b = DWORD(xmidi[offset+1]);

				switch (command & 0xF0) {

					// Note Off
					case 0x80:
						offset += 2;

						m_NoteQueue.InsertNote(trackTime, command | (a << 8) | (b << 16));
						break;

					// Note On
					case 0x90:
						offset += 2;

						bits = DWORD(xmidi[offset++]);

						noteDuration = bits & 0x7F;

						if (0x80 & bits) {
							bits = DWORD(xmidi[offset++]);
							noteDuration = (noteDuration << 7) | (bits & 0x7F);
						}

						m_NoteQueue.InsertNote(trackTime,				(0x90) | (command & 0x0F) | (a << 8) | (b << 16));
						m_NoteQueue.InsertNote(trackTime+noteDuration,	(0x80) | (command & 0x0F) | (a << 8) | (b << 16));
						break;

					// Key after-touch
					case 0xA0:
						offset += 2;
						break;

					// Control change
					case 0xB0:
						offset += 2;

						if (116 == a) {
							printf("\nrepeat set %d\n", baseCommandOffset);
							repeatValid  = true;
							repeatOffset = offset;
						}
						else if (117 == a) {
							if (repeatValid) {
								printf("repeating\n");
								offset = repeatOffset;
							}
						}
						else {
							m_NoteQueue.InsertNote(trackTime, command | (a << 8) | (b << 16));
						}
						break;

					// Program (patch) change
					case 0xC0:
						offset += 1;
						m_NoteQueue.InsertNote(trackTime, command | (a << 8));
						break;

					// Channel after-touch
					case 0xD0:
						offset += 1;
						m_NoteQueue.InsertNote(trackTime, command | (a << 8));
						break;

					// Pitch wheel change
					case 0xE0:
						offset += 2;
						m_NoteQueue.InsertNote(trackTime, command | (a << 8) | (b << 16));
						break;
				}
			}
			else if (0xFF == command) {
//				DWORD meta       = DWORD(xmidi[offset++]);
				DWORD metaLength = DWORD(xmidi[offset++]);
				offset += metaLength;

//				printf("%02X %02X\n", meta, metaLength);
			}
			else {
				switch (command) {
					case 0xF8:
						printf("timing clock\n");
						break;

					case 0xFA:
						printf("start current sequence\n");
						break;

					case 0xFB:
						printf("continue stopped sequence\n");
						break;

					case 0xFC:
						printf("stop sequence\n");
						break;

					case 0xFF:
						printf("NOP\n");
						break;

					default:
						printf("unknown command 0x%02X, 0x%02X, [%d]\n", command, ticks, offset);
						break;
				}
			}
		}

		DWORD currentTime = (timeGetTime() - baseTime) / 8;

		DWORD count = 0;

		for ( ; count < m_NoteQueue.m_QueueDepth; ++count) {
			if (m_NoteQueue.m_Queue[count].Ticks < currentTime) {
//				printf("%6d %08X\n", m_NoteQueue[count].Ticks, m_NoteQueue[count].Message);
				midiOutShortMsg(HMIDIOUT(hStream), m_NoteQueue.m_Queue[count].Message);
			}
			else {
				break;
			}
		}

		if (count > 0) {
			printf("%4d", count);
			m_NoteQueue.PopNotes(count);
		}
		else {
			Sleep(10);
		}

		if (_kbhit()) {
			getch();
			printf("-------\n");
			for (DWORD i = 0; i < m_NoteQueue.m_QueueDepth; ++i) {
				printf("%6d %08X\n", m_NoteQueue.m_Queue[i].Ticks, m_NoteQueue.m_Queue[i].Message);
			}
			loop = false;
		}
	}

	midiStreamClose(hStream);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpEvents()
//
void CwaMidiPlayer::DumpEvents(DWORD notes[], DWORD wordCount)
{
	FILE *pLog = fopen("events.txt", "w");

	DWORD index = 0;

	DWORD currentTime = 0;
	DWORD a, b, c, d, e;

	while (index < wordCount) {
		DWORD ticks = notes[index];
		DWORD eventSize = 3;
		DWORD dataCount = 0;

		currentTime += ticks;

		if (MEVT_F_LONG & notes[index+2]) {
			dataCount = notes[index+2] & 0xFF;
			eventSize = 3 + ((dataCount + 3) / 4);
			m_Command = notes[index+3] & 0xFF;
			a = (notes[index+3] >>  8) & 0xFF;
			b = (notes[index+3] >> 16) & 0xFF;
			c = (notes[index+3] >> 24) & 0xFF;
			d = (notes[index+4]      ) & 0xFF;
			e = (notes[index+4]      ) & 0xFF;
		}
		else {
			m_Command = notes[index+2] & 0xFF;
			a = (notes[index+2] >>  8) & 0xFF;
			b = (notes[index+2] >> 16) & 0xFF;
			c = (notes[index+2] >> 24) & 0xFF;
		}

		if ((m_Command >= 0x80) && (m_Command <= 0xEF)) {
			switch (m_Command & 0xF0) {
				// Note Off
				case 0x80:
					fprintf(pLog, "%6d  %4d off %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, a, b);
					break;

				// Note On
				case 0x90:
					fprintf(pLog, "%6d  %4d on  %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, a, b);
					break;

				// Key after-touch
				case 0xA0:
					fprintf(pLog, "%6d  %4d key %d\n", currentTime, ticks, m_Command & 0xF);
					break;

				// Control change
				case 0xB0:
					fprintf(pLog, "%6d  %4d control %3d, %3d, %3d\n", currentTime, ticks, m_Command & 0xF, a, b);
					break;

				// Program (patch) change
				case 0xC0:
					fprintf(pLog, "%6d  %4d program %3d, %3d\n", currentTime, ticks, m_Command & 0xF, a);
					break;

				// Channel after-touch
				case 0xD0:
					fprintf(pLog, "%6d  %4d channel %d\n", currentTime, ticks, m_Command & 0xF);
					break;

				// Pitch wheel change
				case 0xE0:
					fprintf(pLog, "%6d  %4d pitch %d\n", currentTime, ticks, m_Command & 0xF);
					break;
			}
		}
		else {
			printf("meta: %02X, %d: %02X:%02X:%02X:%02X\n", m_Command, a, b, c, d, e);

			switch (m_Command) {
				case 0x00:	// Sets the track's sequence number
					fprintf(pLog, "Sequence number: %d bytes\n", dataCount);
					break;

				case 0x01:	// Text event
					break;

				case 0x02:
					break;

				case 0x03:
					break;

				case 0x04:
					break;

				case 0x05:
					break;

				case 0x06:
					break;

				case 0x07:
					break;

				case 0x2F:	// end of track
					fprintf(pLog, "End of track\n");
					break;

				case 0x51:	// Set tempo
					fprintf(pLog, "set tempo: %d bytes\n", dataCount);
					break;

				case 0x54:	// SMPTE offset
					fprintf(pLog, "SMPTE offset: %d bytes\n", dataCount);
					break;

				case 0x58:	// Time Signature
					fprintf(pLog, "time signature: %d bytes\n", dataCount);
					break;

				case 0x59:	// Key signature
					fprintf(pLog, "key signature\n");
					break;

				case 0x7F:	// Sequencer specific information
					fprintf(pLog, "sequence data: %d bytes\n", dataCount);
					break;

				case 0xF8:
					fprintf(pLog, "timing clock\n");
					break;

				case 0xFA:
					fprintf(pLog, "start current sequence\n");
					break;

				case 0xFB:
					fprintf(pLog, "continue stopped sequence\n");
					break;

				case 0xFC:
					fprintf(pLog, "stop sequence\n");
					break;

				case 0xFF:
					fprintf(pLog, "NOP\n");
					break;

				default:
					fprintf(pLog, "unknown command 0x%02X, 0x%02X\n", m_Command, ticks);
					break;
			}
		}

		index += eventSize;
	}

	fclose(pLog);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::ParseXmidiFile(char filename[], FILE *pLog)
{
	FILE *pFile = fopen(filename, "rb");
	if (NULL == pFile) {
		return;
	}

	fseek(pFile, 0, SEEK_END);
	DWORD size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	BYTE *pBuffer = new BYTE[size];

	fread(pBuffer, size, 1, pFile);

	fclose(pFile);

	m_TrackCount = 1;

	DWORD trackNumber = 0;

	ParseXmidiData(pBuffer, size, 0, trackNumber, pLog, NULL);

	delete [] pBuffer;
}


/////////////////////////////////////////////////////////////////////////////
//
void Indent(FILE *pFile, DWORD level)
{
	for (DWORD i = 0; i < level; ++i) { fprintf(pFile, "    "); }
}


/////////////////////////////////////////////////////////////////////////////
//
void HexDump(BYTE *pData, DWORD count)
{
	for (DWORD i = 0; i < count; ++i) {
		if (i > 0) {
			printf(":");
		}
		printf("%02X", pData[i]);
	}
	printf("\n");
}


/////////////////////////////////////////////////////////////////////////////
//
//	ParseXmidiData()
//
void CwaMidiPlayer::ParseXmidiData(BYTE *pBuffer, DWORD bufferSize, DWORD level, DWORD &trackNumber, FILE *pLog, FILE *pMidi)
{
	DWORD chunkSize;
	DWORD offset = 0;
	DWORD type[2];
	type[1] = 0;

	while (offset < bufferSize) {
		DWORD chunkID = *reinterpret_cast<DWORD*>(pBuffer + offset);

		offset += 4;

		switch (chunkID) {
			case FourCC_FORM:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;

				type[0] = *reinterpret_cast<DWORD*>(pBuffer + offset);
				if (type[0] == FourCC_XDIR) {
				}
				else if (type[0] == FourCC_XMID) {
				}
				else {
					fprintf(pLog, "FORM did not see XDIR or XMID tag: %s\n", (char*)type);
					return;
				}

				Indent(pLog, level);
				fprintf(pLog, "FORM chunk [%s]: %d bytes\n", (char*)type, chunkSize);

				ParseXmidiData(pBuffer + offset + 4, chunkSize - 4, level + 1, trackNumber, pLog, pMidi);

				offset += chunkSize;
				break;

			case FourCC_EVNT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;

				Indent(pLog, level);
				fprintf(pLog, "EVNT chunk[%d]: %d bytes\n", trackNumber, chunkSize);
				++trackNumber;

				ParseXmidiTrack(pBuffer + offset, chunkSize, pLog, pMidi);

				offset += chunkSize;
				break;

			case FourCC_INFO:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;

				Indent(pLog, level);
				fprintf(pLog, "INFO chunk: %d bytes\n", chunkSize);

				Indent(pLog, level+1);
				HexDump(pBuffer+offset, chunkSize);

				if (chunkSize >= 2) {
					m_TrackCount = SwizzleWORD(pBuffer + offset);
				}

				offset += chunkSize;
				break;

			case FourCC_CAT:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;

				type[0] = *reinterpret_cast<DWORD*>(pBuffer + offset);
				if (type[0] == FourCC_XMID) {
				}
				else {
					fprintf(pLog, "FORM did not see XDIR or XMID tag: %s\n", (char*)type);
					return;
				}

				Indent(pLog, level);
				fprintf(pLog, "CAT chunk: %d bytes\n", chunkSize);

				ParseXmidiData(pBuffer + offset + 4, chunkSize - 4, level + 1, trackNumber, pLog, pMidi);

				offset += chunkSize;
				break;

			case FourCC_RBRN:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;
				Indent(pLog, level);
				fprintf(pLog, "RBRN chunk: %d bytes\n", chunkSize);
				offset += chunkSize;
				break;

			case FourCC_TIMB:
				chunkSize = SwizzleDWORD(pBuffer + offset);
				offset += 4;

				Indent(pLog, level);
				fprintf(pLog, "TIMB chunk: %d bytes\n", chunkSize);

				Indent(pLog, level+1);
				HexDump(pBuffer+offset, chunkSize);

				offset += chunkSize;
				break;

			default:
				fprintf(pLog, "Unknown chunk type: 0x%08X\n", chunkID);
				return;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::ParseXmidiTrack(BYTE *pTrack, DWORD trackLength, FILE *pLog, FILE *pMidi)
{
	DWORD m_Command = 0;
	DWORD trackOffset = 0;

	DWORD noteCount = 0;

	DWORD loopAddress = 0;
	DWORD currentTime = 0;

	DWORD ticks = 0;

	while (trackOffset < trackLength) {
		DWORD startOffset = trackOffset;
/*
		fprintf(pLog, "     %02X:%02X:%02X:%02X:%02X:%02X\n",
			pTrack[trackOffset],
			pTrack[trackOffset+1],
			pTrack[trackOffset+2],
			pTrack[trackOffset+3],
			pTrack[trackOffset+4],
			pTrack[trackOffset+5]);
*/
		ticks = 0;

		while (0 == (0x80 & pTrack[trackOffset])) {
			ticks += DWORD(pTrack[trackOffset++]);
		}

		currentTime += ticks;

		if (0 != (0x80 & pTrack[trackOffset])) {
			m_Command = DWORD(pTrack[trackOffset++]);
//			fprintf(pLog, "command: %d\n", m_Command);
		}

		if ((m_Command >= 0x80) && (m_Command <= 0xEF)) {
			DWORD a = DWORD(pTrack[trackOffset]);
			DWORD b = DWORD(pTrack[trackOffset+1]);

			switch (m_Command & 0xF0) {

				// Note Off
				case 0x80:
					fprintf(pLog, "\t\t\t0x%06X: %6d  %4d off %d, %d, %d\n", startOffset, currentTime, ticks, m_Command & 0xF, a, b);

					trackOffset += 2;
					break;

				// Note On
				case 0x90:
					trackOffset += 2;

					{
//						DWORD channel = m_Command & 0xF;
						DWORD c       = DWORD(pTrack[trackOffset++]);
						DWORD delta   = c & 0x7F;

						if (c & 0x80) {
							c     = DWORD(pTrack[trackOffset++]);
							delta = (delta << 7) | (c & 0x7F);
						}

						delta *= 2;

						DWORD d = 0;//DWORD(pTrack[trackOffset++]);

						fprintf(pLog, "\t\t\t0x%06X: %6d  %4d-%-3d on  %d, %d, %d, %d, %d\n", startOffset, currentTime, ticks, delta, m_Command & 0xF, a, b, c, d);

						++noteCount;
					}

					break;

				// Key after-touch
				case 0xA0:
					fprintf(pLog, "\t\t\t0x%06X:   %4d key %d\n", startOffset, ticks, m_Command & 0xF);

					trackOffset += 2;
					break;

				// Control change
				case 0xB0:
					fprintf(pLog, "\t\t\t0x%06X: %6d  %4d control %d, %d, %d\n", startOffset, currentTime, ticks, m_Command & 0xF, pTrack[trackOffset], pTrack[trackOffset+1]);

					trackOffset += 2;

					if (116 == a) {
						fprintf(pLog, "\t\t\t       set loop address\n");
						loopAddress = startOffset;
					}
					else if (117 == a) {
						fprintf(pLog, "\t\t\t       loop back to 0x%06X\n", loopAddress);
					}
					break;

				// Program (patch) change
				case 0xC0:
					fprintf(pLog, "\t\t\t0x%06X:   %4d program %d, %d\n", startOffset, ticks, m_Command & 0xF, pTrack[trackOffset]);

					trackOffset += 1;
					break;

				// Channel after-touch
				case 0xD0:
					fprintf(pLog, "\t\t\t0x%06X:   %4d channel %d\n", startOffset, ticks, m_Command & 0xF);

					trackOffset += 1;
					break;

				// Pitch wheel change
				case 0xE0:
					fprintf(pLog, "\t\t\t0x%06X:   %4d pitch %d\n", startOffset, ticks, m_Command & 0xF);

					trackOffset += 2;
					break;
			}
		}
		else if (0xFF == m_Command) {
			DWORD meta = DWORD(pTrack[trackOffset++]);

			switch (meta) {
				case 0x00:	// Sets the track's sequence number
					fprintf(pLog, "\t\t\t0x%06X: Sequence number\n", startOffset);

					trackOffset += 3;
					break;

				case 0x01:	// Text event
					trackOffset += PrintCommand(pTrack + trackOffset, "Comment:");
					break;

				case 0x02:
					trackOffset += PrintCommand(pTrack + trackOffset, "Copyright:");
					break;

				case 0x03:
					trackOffset += PrintCommand(pTrack + trackOffset, "Track Name:");
					break;

				case 0x04:
					trackOffset += PrintCommand(pTrack + trackOffset, "Instrument Name:");
					break;

				case 0x05:
					trackOffset += PrintCommand(pTrack + trackOffset, "Lyric:");
					break;

				case 0x06:
					trackOffset += PrintCommand(pTrack + trackOffset, "Marker:");
					break;

				case 0x07:
					trackOffset += PrintCommand(pTrack + trackOffset, "Cue Point:");
					break;

				case 0x2F:	// end of track
					fprintf(pLog, "\t\t\t0x%06X: End of track\n", startOffset);
					trackOffset += 1;
					break;

				case 0x51:	// Set tempo
					{
						DWORD tempo = DWORD(pTrack[trackOffset+3])
							| (DWORD(pTrack[trackOffset+2]) << 8)
							| (DWORD(pTrack[trackOffset+1]) << 16);

						fprintf(pLog, "\t\t\t0x%06X: tempo = 0x%08X = %d\n", startOffset, tempo, tempo);
					}

					trackOffset += 4;
					break;

				case 0x54:	// SMPTE offset
					fprintf(pLog, "\t\t\t0x%06X: SMPTE offset\n", startOffset);

					trackOffset += 6;
					break;

				case 0x58:	// Time Signature
//					fprintf(pLog, "time signature\n");

					{
						BYTE *p = pTrack+trackOffset-1;
fprintf(pLog, "\t\t\t0x%06X: time sig: %02X, %d: %02X:%02X:%02X:%02X\n", startOffset, p[0], p[1], p[2], p[3], p[4], p[5]);
//printf("timesig = 0x%08X\n", *((DWORD*)&(entries[noteCount].Data[2])));
					}

					trackOffset += 5;
					break;

				case 0x59:	// Key signature
					fprintf(pLog, "\t\t\t0x%06X: key signature\n", startOffset);
					trackOffset += 3;
					break;

				case 0x7F:	// Sequencer specific information
					fprintf(pLog, "\t\t\t0x%06X: sequence data\n", startOffset);
					trackOffset += 1 + DWORD(pTrack[trackOffset]);
					break;

				default:
					fprintf(pLog, "\t\t\t0x%06X: unknown meta 0x%02X\n", startOffset, meta);
					break;
			}
		}
		else {
			switch (m_Command) {
				case 0xF8:
					fprintf(pLog, "\t\t\t0x%06X: timing clock\n", startOffset);
					break;

				case 0xFA:
					fprintf(pLog, "\t\t\t0x%06X: start current sequence\n", startOffset);
					break;

				case 0xFB:
					fprintf(pLog, "\t\t\t0x%06X: continue stopped sequence\n", startOffset);
					break;

				case 0xFC:
					fprintf(pLog, "\t\t\t0x%06X: stop sequence\n", startOffset);
					break;

				case 0xFF:
					fprintf(pLog, "\t\t\t0x%06X: NOP\n", startOffset);
					break;

				default:
					fprintf(pLog, "\t\t\t0x%06X: unknown command 0x%02X, 0x%02X\n", startOffset, m_Command, ticks);
					break;
			}
		}
	}

	fprintf(pLog, "    events = %d\n", noteCount);
	fprintf(pLog, "    end ticks = %d\n", currentTime);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::SetTempo(DWORD tempo)
{
	m_Tempo = tempo;
}


/////////////////////////////////////////////////////////////////////////////
//
bool CwaMidiPlayer::IsSongFinished(void)
{
	return (false == m_IsThreadPlaying);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueueSong(CwaMidiSong *pSong, MidiEvent_t *pEvents, DWORD trackCount, bool startNow)
{
	EnterCriticalSection(&m_MidiSection);

	if (startNow) {
		QueueStop();
	}

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_Song;
		m_Queue[idx].pSong      = pSong;
		m_Queue[idx].pEvents    = pEvents;
		m_Queue[idx].EventCount = trackCount;
	}

	if (startNow) {
		QueueStart();
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueueStart(void)
{
	EnterCriticalSection(&m_MidiSection);

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_Start;
		m_Queue[idx].pSong      = NULL;
		m_Queue[idx].pEvents    = NULL;
		m_Queue[idx].EventCount = 0;
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueueStop(void)
{
	EnterCriticalSection(&m_MidiSection);

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_Stop;
		m_Queue[idx].pSong      = NULL;
		m_Queue[idx].pEvents    = NULL;
		m_Queue[idx].EventCount = 0;
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueuePause(void)
{
	EnterCriticalSection(&m_MidiSection);

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_Pause;
		m_Queue[idx].pSong      = NULL;
		m_Queue[idx].pEvents    = NULL;
		m_Queue[idx].EventCount = 0;
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueueVolume(DWORD volume)
{
	EnterCriticalSection(&m_MidiSection);

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_Volume;
		m_Queue[idx].pSong      = NULL;
		m_Queue[idx].pEvents    = NULL;
		m_Queue[idx].EventCount = 0;
		m_Queue[idx].Value      = volume;
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
void CwaMidiPlayer::QueueNextEvent(DWORD id)
{
	EnterCriticalSection(&m_MidiSection);

	if (m_QueueCount < ArraySize(m_Queue)) {
		DWORD idx = (m_QueueIndex + m_QueueCount) % ArraySize(m_Queue);

		++m_QueueCount;

		m_Queue[idx].Command    = MidiQueue_NextEvent;
		m_Queue[idx].pSong      = NULL;
//		m_Queue[idx].ID         = id;
		m_Queue[idx].pEvents    = NULL;
		m_Queue[idx].EventCount = 0;
	}

	LeaveCriticalSection(&m_MidiSection);
}


/////////////////////////////////////////////////////////////////////////////
//
//	StartThread()
//
bool CwaMidiPlayer::StartThread(void)
{
	StopThread();

	ResetEvent(m_hThreadEvent);

	m_QueueIndex = 0;
	m_QueueCount = 0;

	m_hThread = HANDLE(_beginthreadex(NULL, 0, JumpStartThread, this, 0, (unsigned int*)(&m_ThreadID)));

	if (NULL == m_hThread) {
		m_ThreadID = 0;
		return false;
	}

	SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST);

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	StopThread()
//
void CwaMidiPlayer::StopThread(void)
{
	if (NULL != m_hThread) {
		SetEvent(m_hThreadEvent);

		WaitForSingleObject(m_hThread, INFINITE);

		CloseHandle(m_hThread);

		m_hThread  = NULL;
		m_ThreadID = 0;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	JumpStartThread()
//
unsigned __stdcall CwaMidiPlayer::JumpStartThread(void *pContext)
{
	return reinterpret_cast<CwaMidiPlayer*>(pContext)->WorkerThread();
}


/////////////////////////////////////////////////////////////////////////////
//
//	WorkerThread()
//
DWORD CwaMidiPlayer::WorkerThread(void)
{
	LogMessage("worker thread start");

	MMRESULT result;

	HMIDISTRM hStream  = 0;
	UINT      deviceID = 0;

	result = midiStreamOpen(&hStream, &deviceID, 1, 0, 0, CALLBACK_NULL);

	MIDIPROPTIMEDIV prop;
	prop.cbStruct  = sizeof(MIDIPROPTIMEDIV);
	prop.dwTimeDiv = 384;
	result = midiStreamProperty(hStream, (LPBYTE)&prop, MIDIPROP_SET|MIDIPROP_TIMEDIV);

	MIDIPROPTEMPO  propTempo;
	propTempo.cbStruct = sizeof(MIDIPROPTEMPO);
	propTempo.dwTempo  = 250000;
	result = midiStreamProperty(hStream, (LPBYTE)&propTempo, MIDIPROP_SET|MIDIPROP_TEMPO);

	// On a scale of 0-100.
	DWORD currentVolume = 50;

	DWORD midiVolume = (currentVolume * 0xFFFF) / 100;
	result = midiOutSetVolume((HMIDIOUT)hStream, midiVolume | (midiVolume << 16));

	// If the request to set the volume control failed, the sound card has some
	// problem.  This frequently fails with SoundBlasters, so manual control of
	// volume settings will be required.
	bool useVolumeControl = true;
	if (MMSYSERR_NOERROR != result) {
		useVolumeControl = false;
	}

	m_IsThreadPlaying = false;

	DWORD baseTime		= 0;
	m_TrackTime			= 0;
	m_EventOffset		= 0;

	DWORD noteDuration = 0;
	DWORD bits = 0;

	DWORD command = 0;

	m_NoteQueue.Reset();

	bool playing			= false;
//	bool stopping			= false;

	m_pSong				= NULL;
	m_pEventList		= NULL;

	m_RepeatValid		= false;
	m_RepeatOffset		= 0;
	m_TotalEventCount	= 0;
	m_EventLoopCount	= 1;
	m_EventIndex		= 0;
	m_EventSize			= 0;
	m_pEventStart		= NULL;
	m_FirstNoteSeen		= false;

	for (;;) {
		DWORD result = WaitForSingleObject(m_hThreadEvent, 10);

		if (WAIT_OBJECT_0 == result) {
			LogMessage("graceful thread exit");
			break;
		}
		else if (WAIT_TIMEOUT != result) {
			LogMessage("fatal thread exit");
			break;
		}

		while (m_QueueCount > 0) {
			EnterCriticalSection(&m_MidiSection);

			if (m_QueueCount < ArraySize(m_Queue)) {
				DWORD idx = m_QueueIndex;

				--m_QueueCount;

				m_QueueIndex = (m_QueueIndex + 1) % ArraySize(m_Queue);

				if (MidiQueue_Song == m_Queue[idx].Command) {
					LogMessage("queuing new song");
					m_pSong				= m_Queue[idx].pSong;
					m_pEventList		= m_Queue[idx].pEvents;
                    m_TotalEventCount	= m_Queue[idx].EventCount;
					m_EventIndex		= 0;
					m_EventLoopCount	= 1;
					m_pEventStart		= NULL;
					m_FirstNoteSeen		= false;
				}
				else if (MidiQueue_Start == m_Queue[idx].Command) {
					LogMessage("starting song");
					playing = true;
			baseTime       = timeGetTime();
			m_TrackTime    = 0;
				}
				else if (MidiQueue_Stop == m_Queue[idx].Command) {
					LogMessage("stopping song");
					playing = false;
				}
				else if (MidiQueue_Pause == m_Queue[idx].Command) {
					LogMessage("pausing song");
					playing = false;
				}
				else if (MidiQueue_Volume == m_Queue[idx].Command) {
					LogMessage("volume = %d", m_Queue[idx].Value);
					currentVolume = m_Queue[idx].Value;
					midiVolume = (currentVolume * 0xFFFF) / 100;
					if (midiVolume > 0xFFFF) {
						midiVolume = 0xFFFF;
					}
					midiOutSetVolume((HMIDIOUT)hStream, midiVolume | (midiVolume << 16));
				}
				else if (MidiQueue_NextEvent == m_Queue[idx].Command) {
//					nextEventIndex = m_Queue[idx].ID;
//					LogMessage("next event: %d", nextEvent);
				}
			}

			LeaveCriticalSection(&m_MidiSection);
		}

		if ((NULL == m_pEventStart) && (NULL != m_pSong)) {
			m_EventIndex     = 0;
			m_pEventStart    = m_pSong->GetEvent(m_pEventList[m_EventIndex].EventNumber, m_EventSize);
			m_EventLoopCount = 1;
			m_EventOffset    = 0;
			m_RepeatValid    = false;

			m_IsThreadPlaying = true;

			LogMessage("loading event %d, %d bytes", m_pEventList[m_EventIndex].EventNumber, m_EventSize);
		}
/*
		if ((NULL != pCurrentSong) && ((offset + 1) >= eventSize)) {
//			currentEvent = nextEvent;
			++currentEvent;
			pEventStart = pCurrentSong->GetEvent(currentEvent, eventSize);
			offset = 0;
		}
*/
		// Any time the queue ends up empty, reset the current time.
/*
		if (0 == m_QueueDepth) {
			LogMessage("zero queue");
			baseTime       = timeGetTime();
			m_TrackTime    = 0;
		}
*/
		while (playing &&
			   (NULL != m_pEventStart) &&
			   ((m_EventOffset + 1) < m_EventSize) &&
			   (m_NoteQueue.HasSpace()))
		{
			DWORD baseCommandOffset = m_EventOffset;

			DWORD ticks = 0;

			while (0 == (0x80 & m_pEventStart[m_EventOffset])) {
//				LogMessage("++++: %d   %d/%d\n", pEventStart[offset], offset, eventSize);
				ticks += DWORD(m_pEventStart[m_EventOffset++]);
			}
/*
			if (0xB0 == (0xF0 & m_pEventStart[m_EventOffset])) {
				if (ticks > 20) {
					LogMessage("clamping %d [%02X]\n", ticks, DWORD(m_pEventStart[m_EventOffset+1]));
					ticks = 20;
				}
			}
*/
			if (0 != (0x80 & m_pEventStart[m_EventOffset])) {
				command = DWORD(m_pEventStart[m_EventOffset++]);
			}

			// Only count time after we've seen the first note.
			// This effectively skips the control/program operations at
			// the start of each event that can cause seconds-long pauses.
			if (m_FirstNoteSeen) {
				m_TrackTime += ticks;
			}

//			LogMessage("time: %6d %02X %02X, %d/%d\n", trackTime, command, DWORD(pEventStart[offset]), offset, eventSize);

			if ((command >= 0x80) && (command <= 0xEF)) {
				DWORD a = DWORD(m_pEventStart[m_EventOffset]);
				DWORD b = DWORD(m_pEventStart[m_EventOffset+1]);

				switch (command & 0xF0) {

					// Note Off
					case 0x80:
						m_EventOffset += 2;

						m_NoteQueue.InsertNote(m_TrackTime, command | (a << 8) | (b << 16));
						break;

					// Note On
					case 0x90:
						m_EventOffset += 2;

						bits = DWORD(m_pEventStart[m_EventOffset++]);

						noteDuration = bits & 0x7F;

						if (0x80 & bits) {
							bits = DWORD(m_pEventStart[m_EventOffset++]);
							noteDuration = (noteDuration << 7) | (bits & 0x7F);
						}

						m_NoteQueue.InsertNote(m_TrackTime,					(0x90) | (command & 0x0F) | (a << 8) | (b << 16));
						m_NoteQueue.InsertNote(m_TrackTime+noteDuration,	(0x80) | (command & 0x0F) | (a << 8) | (b << 16));

						m_FirstNoteSeen = true;
						break;

					// Key after-touch
					case 0xA0:
						m_EventOffset += 2;
						break;

					// Control change
					case 0xB0:
						m_EventOffset += 2;

						if ((7 == a) && (false == useVolumeControl)) {
							b = (b * currentVolume) / 100;
						}

						if (116 == a) {
							LogMessage("repeat set %d of %d", baseCommandOffset, m_EventSize);
							m_RepeatValid  = true;
							m_RepeatOffset = m_EventOffset;
						}
						else if (117 == a) {
							HandleEndOfEvent();
						}
						else {
//LogMessage("ctrl %d\n", a);
							m_NoteQueue.InsertNote(m_TrackTime, command | (a << 8) | (b << 16));
						}
						break;

					// Program (patch) change
					case 0xC0:
						m_EventOffset += 1;
						m_NoteQueue.InsertNote(m_TrackTime, command | (a << 8));
						break;

					// Channel after-touch
					case 0xD0:
						m_EventOffset += 1;
						m_NoteQueue.InsertNote(m_TrackTime, command | (a << 8));
						break;

					// Pitch wheel change
					case 0xE0:
						m_EventOffset += 2;
						m_NoteQueue.InsertNote(m_TrackTime, command | (a << 8) | (b << 16));
						break;
				}
			}
			else if (0xFF == command) {
				DWORD meta       = DWORD(m_pEventStart[m_EventOffset++]);
				DWORD metaLength = DWORD(m_pEventStart[m_EventOffset++]);

				m_EventOffset += metaLength;

//				LogMessage("meta: %02X %02X\n", meta, metaLength);
				switch (meta) {
					case 0x2F:	// end of track
						HandleEndOfEvent();
						break;
				}
			}
			else {
				switch (command) {
					case 0xF8:
						LogMessage("timing clock");
						break;

					case 0xFA:
						LogMessage("start current sequence");
						break;

					case 0xFB:
						LogMessage("continue stopped sequence");
						break;

					case 0xFC:
						LogMessage("stop sequence");
						break;

					case 0xFF:
						LogMessage("NOP");
						break;

					default:
						LogMessage("unknown command 0x%02X, 0x%02X, [%d]", command, ticks, m_EventOffset);
						break;
				}
			}
		}

		DWORD currentTime = timeGetTime() - baseTime;

		DWORD count = 0;

		for ( ; count < m_NoteQueue.m_QueueDepth; ++count) {
			DWORD targetTime = (m_NoteQueue.m_Queue[count].Ticks * 1000) / m_Tempo;
			if (targetTime < currentTime) {
//				LogMessage("%8d %08X\n", m_NoteQueue[count].Ticks, m_NoteQueue[count].Message);
				midiOutShortMsg(HMIDIOUT(hStream), m_NoteQueue.m_Queue[count].Message);
			}
			else {
				break;
			}
		}

		if (count > 0) {
			m_NoteQueue.PopNotes(count);
		}
	}

	midiStreamClose(hStream);

	LogMessage("worker thread exit");

	return 0;
}


void CwaMidiPlayer::HandleEndOfEvent(void)
{
	if (m_EventLoopCount < m_pEventList[m_EventIndex].RepeatCount) {
		LogMessage("repeating");
		++m_EventLoopCount;
		if (m_RepeatValid) {
			m_EventOffset = m_RepeatOffset;
		}
		else {
			m_EventOffset = 0;
		}
	}
	else if ((m_EventIndex+1) < m_TotalEventCount) {
		++m_EventIndex;
		LogMessage("advancing to event %d instead of repeating", m_pEventList[m_EventIndex].EventNumber);
		m_EventLoopCount = 1;
		m_EventOffset    = 0;
		m_FirstNoteSeen  = false;
		m_pEventStart    = m_pSong->GetEvent(m_pEventList[m_EventIndex].EventNumber, m_EventSize);
		m_RepeatValid    = false;
		m_RepeatOffset   = 0;
	}
	else {
		LogMessage("end of song");

		m_pSong           = NULL;
		m_pEventStart     = NULL;

		m_IsThreadPlaying = false;
	}
}




