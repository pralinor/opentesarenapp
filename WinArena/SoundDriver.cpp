/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/SoundDriver.cpp 5     12/30/05 1:14a Lee $
//
//	File: SoundDriver.cpp
//
//
//	This is a very basic class for playing streaming audio using the waveOut
//	Win32 functions.  It is pretty much tied into playing 16-bit, mono sound
//	clips only.  It could be improved upon in a number of ways, but is good
//	enough for playing back audio data extracted from Global.BSA.
//
//	The Arena sounds themselves have frequencies from around 5 KHz to 11 KHz,
//	and are all 8-bit mono.  The audio quality is pretty poor, and trying to
//	play back clips of different sample rates is difficult.  Arena delta with
//	that problem by never playing back more than one clip at a time, which
//	meant if it was playing back a footstep sound, you wouldn't hear the
//	fireball some lich just lobbed at your backside.
//
//	This program will load the raw audio data, and do some resampling and
//	filtering to convert it to 44,100 Hz, 16-bit mono.  Real-time mixing will
//	allow multiple sounds to be played simultaneously.  Since filtering the
//	audio is very slow, it is done outside the sound driver, so that the
//	resampled data can be cached, rather than forcing it to be generated every
//	time the program starts up.
//
//	This uses the old Windows multimedia extensions library to play sounds.
//	It will continually play sound to the speaker.  In the absense of any
//	sound clips, it will play silence.  But it is always playing.
//
//	Using the library requires handing it a set of sounds, using the
//	AssignSound() method.  Each sound is assigned an ID (actually, an index
//	into an array), which is used to select sound when playing audio.  Sounds
//	can be looped -- "Halt! Halt! Halt!"  Up to 8 sounds could be played
//	simultaneously (more, if c_MaxConcurrentSounds is changed), but more than
//	a couple of sounds at a time are likely to cause clipping errors when
//	mixing them together.  Linear vs. non-linear issues are ignored when
//	mixing, since the results seem to sound fine (and is better than Arena,
//	which could not play more than one sound at a time, resulting in sounds
//	being dropped).
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#include "WinArena.h"
#include "SoundDriver.h"


CwaSoundDriver* g_pSoundDriver;


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaSoundDriver::CwaSoundDriver()
	:	m_SampleRate(c_AudioSampleRate),
		m_IsAudioPlaying(false),
		m_CurrentBuffer(0),
		m_pMixer(NULL),
		m_SoundCount(0)
{
	memset(&m_SoundList, 0, sizeof(m_SoundList));
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaSoundDriver::~CwaSoundDriver()
{
	Stop();
}


/////////////////////////////////////////////////////////////////////////////
//
//	Start()
//
//	Turns on the driver.  This inits the buffering system, and fires up the
//	background multimedia thread used by WinMM to play audio.
//
bool CwaSoundDriver::Start(void)
{
	g_pSoundDriver = this;

	// Number of milliseconds' worth of audio to fit in each buffer.
	const c_SampleInterval = 100;

	m_BufferSize  = (c_SampleInterval * m_SampleRate) / 1000;
	m_BufferSize /= c_BufferCount;

	for (DWORD i = 0; i < c_MaxConcurrentSounds; ++i) {
		m_ActiveSounds[i].SoundID       = DWORD(-1);
		m_ActiveSounds[i].RepeatCount   = 0;
		m_ActiveSounds[i].Volume        = 0;
		m_ActiveSounds[i].CurrentOffset = 0;
	}

	WAVEFORMATEX format;
	format.wFormatTag		= 1;		// PCM standard.
	format.nChannels		= 1;		// Mono
	format.nSamplesPerSec	= m_SampleRate;
	format.nAvgBytesPerSec	= m_SampleRate;
	format.nBlockAlign		= WORD(2);
	format.wBitsPerSample	= WORD(16);
	format.cbSize			= 0;

	MMRESULT errCode = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &format, DWORD_PTR(CallbackHandler), 0, CALLBACK_FUNCTION);

	if (MMSYSERR_NOERROR != errCode) {
		return false;
	}

	// Allocate scratch buffers where chunks of audio data will be stored.
	// This is the data passed into the waveOut...() functions.
	for (DWORD i = 0; i < c_BufferCount; ++i) {
		m_pSoundBuffer[i] = new short[m_BufferSize];
		memset(&m_WaveHeader[i], 0, sizeof(WAVEHDR));
	}

	// Fill all the sound buffers.  No clips should have been queued before
	// starting audio, so all this should do is prep the queuing logic to
	// play back silence.
	m_CurrentBuffer = 0;
	for (i = 0; i < c_BufferCount; ++i) {
		FillNextBuffer();
	}

	// Create a scratch buffer uses to mix multiple tracks of audio data,
	// in case two or more clips end up overlapping.
	m_pMixer = new long[m_BufferSize];

	m_IsAudioPlaying = TRUE;

	// This starts the audio playback process.  After this point, all of
	// the playback and queuing is controlled via the callback function.
	waveOutRestart(m_hWaveOut);

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	Stop()
//
//	Stops the sound driver and cleans up all of the audio buffers used by the
//	playback process.
//
void CwaSoundDriver::Stop(void)
{
	if (m_IsAudioPlaying) {
		g_pSoundDriver = NULL;

		waveOutReset(m_hWaveOut);

		for (DWORD bufferNum = 0; bufferNum < c_BufferCount; ++bufferNum) {
			if (m_WaveHeader[bufferNum].dwFlags & WHDR_PREPARED) {
				waveOutUnprepareHeader(m_hWaveOut, &m_WaveHeader[bufferNum], sizeof(WAVEHDR));
			}

			SafeDeleteArray(m_pSoundBuffer[bufferNum]);
		}

		waveOutClose(m_hWaveOut);

		SafeDeleteArray(m_pMixer);

		m_IsAudioPlaying = false;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	AssignSound()
//
//	Takes a pointer to a chunk of raw PCM audio, along with the size of the
//	buffer in samples.
//
//	NOTE: This code does not assume memory management responsibility of the
//	audio data.  All it stores is the pointer.  It's up to the caller to
//	free buffers after playback is finished.
//
//	An ID is assigned to the sound clip, which is used when playing back
//	the audio data.  This keeps all pointer logic internal.
//
DWORD CwaSoundDriver::AssignSound(short *pSamples, DWORD sampleCount)
{
	if (m_SoundCount < c_MaxSounds) {
		DWORD index = m_SoundCount++;

		m_SoundList[index].pSamples    = pSamples;
		m_SoundList[index].SampleCount = sampleCount;

		return index;
	}

	return DWORD(-1);
}


/////////////////////////////////////////////////////////////////////////////
//
//	QueueSound()
//
//	Requests that a sound clip be played back.  Assuming the maximum number
//	of sounds has not been exceeded, the sound will be queued.  Playback of
//	the sound will begin with the next callback.
//
//	WARNING: We're playing fast 'n loose with multithreading here.  The
//	callback thread ignores a sound clip if the SoundID field is -1, so we
//	can fill in an empty clip without mucking up the multithreading, provided
//	that SoundID is the last field we change.  If a context change occurs
//	while this method is executing, no harm should come if it.
//
void CwaSoundDriver::QueueSound(DWORD id, DWORD repeatCount, DWORD volume)
{
	if (id < c_MaxSounds) {
		for (DWORD i = 0; i < c_MaxConcurrentSounds; ++i) {
			if (DWORD(-1) == m_ActiveSounds[i].SoundID) {
// FIXME: why is sound distorted for volumes > 128?

				m_ActiveSounds[i].RepeatCount   = repeatCount;
				m_ActiveSounds[i].Volume        = volume / 2;
				m_ActiveSounds[i].CurrentOffset = 0;
				m_ActiveSounds[i].SoundID       = id;
				break;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	FillNextBuffer()
//
//	Once playback beings, this method is only called from the callback
//	handler, which is executing in a separate thread from the main app.
//	Therefore caution must be exercised when accessing shared data.  There
//	are special cases where glitching could occur, causing a brief delay
//	in the middle of one clip when another clip is first started, if that
//	new clip is added at just the wrong time.  But that requires interrupting
//	the callback thread with the main thread, and callback threads generally
//	run at a higher priority, for short durations, making this very unlikely.
//
void CwaSoundDriver::FillNextBuffer(void)
{
	MMRESULT result;

	// Make a sanity check on the next buffer.  It should never be in a
	// prepared state.  If it is, something is likely screwed up, be we can
	// try to recover by unpreparing the header, then proceeding blindly in
	// the hope that things won't crash -- that's the nice thing about
	// multithreading problems: you don't generally have to worry about cleaning
	// up, since when they happen your app goes down in flames before you know
	// what happened, let alone have the opportunity to do anything about it.
	// The flip side is you have no clue why it when down in flames.
	//
	if (m_WaveHeader[m_CurrentBuffer].dwFlags & WHDR_PREPARED) {
		result = waveOutUnprepareHeader(m_hWaveOut, &m_WaveHeader[m_CurrentBuffer], sizeof(WAVEHDR));
	}

	// Prepare the next buffer scratch buffer for playback.  Once it has been
	// prepared, we need to fill it quick so we're done with it so there is no
	// delay that would cause a break in audio playback.
	m_WaveHeader[m_CurrentBuffer].lpData         = (char*)m_pSoundBuffer[m_CurrentBuffer];
	m_WaveHeader[m_CurrentBuffer].dwBufferLength = m_BufferSize * sizeof(short);
	result = waveOutPrepareHeader(m_hWaveOut, &m_WaveHeader[m_CurrentBuffer], sizeof(WAVEHDR));

	DWORD i;
	DWORD soundCount = 0;
	DWORD firstSound = DWORD(-1);

	// Scan through the clip list to figure out how many sounds there are.
	// Usually this will be zero, since sounds are infrequent.  Sometimes it
	// will be more than one, which means two or more clips will need to be
	// mixed together.
	for (i = 0; i < c_MaxConcurrentSounds; ++i) {
		if (DWORD(-1) != m_ActiveSounds[i].SoundID) {
			if (m_ActiveSounds[i].CurrentOffset < m_SoundList[m_ActiveSounds[i].SoundID].SampleCount) {
				++soundCount;

				if (DWORD(-1) == firstSound) {
					firstSound = i;
				}
			}
			else {
				m_ActiveSounds[i].SoundID = DWORD(-1);
			}
		}
	}

	// Possible optimization: If there is only one clip of audio, we should
	// just memcpy directly to the destination buffer.  But that requires
	// lots of duplicated logic to deal with partial copies, looping, and
	// zero padding left over space in the buffer.  Plus, it ignores any
	// volume adjustment for this sound.  The mixing logic below does all of
	// that for us, so it's not worth the effort. 

	// No clips are queued.  All we need is to zero out the buffer to continue
	// playing silence.
	if (0 == soundCount) {
		memset(m_pSoundBuffer[m_CurrentBuffer], 0, m_BufferSize * sizeof(short));
	}

	// One or more clips are active.  Mix all of them into a buffer, then
	// use those results to fill in the playback buffer.
	else {
		// Zero out the mixer buffer.
		DWORD i;
		for (i = 0; i < m_BufferSize; ++i) {
			m_pMixer[i] = 0;
		}

		// Loop over all active clips.  We need to add each sound clip to
		// the mixer buffer.  This all assumes that the audio is linear,
		// otherwise we'd need to deal with A-law or mu-law.  Thankfully that
		// is not needed for Arena's sound clips.
		//
		for (DWORD soundNum = 0; soundNum < c_MaxConcurrentSounds; ++soundNum) {
			DWORD soundID = m_ActiveSounds[soundNum].SoundID;

			DWORD copyOffset = 0;
			while ((DWORD(-1) != soundID) && (copyOffset < m_BufferSize)) {
				DWORD copySize = m_SoundList[soundID].SampleCount - m_ActiveSounds[soundNum].CurrentOffset;

				if (copySize > (m_BufferSize - copyOffset)) {
					copySize = m_BufferSize - copyOffset;
				}

				short *pBase = m_SoundList[soundID].pSamples + m_ActiveSounds[soundNum].CurrentOffset;

				for (DWORD idx = 0; idx < copySize; ++idx) {
					m_pMixer[idx+copyOffset] += long(pBase[idx]) * long(m_ActiveSounds[soundNum].Volume);
				}

				copyOffset += copySize;

				m_ActiveSounds[soundNum].CurrentOffset += copySize;

				// Detect if we've hit the end of the buffer.  In the normal
				// case, we're done with the buffer, so we can flag that entry
				// in the sound list as being free so new sounds can be queued
				// to that position in the list.
				//
				// The exception is when sounds are looping.  In that case we
				// decrement the loop count, which will cause this code to
				// repeat, copying more audio from the start of the buffer.
				//
				if (m_ActiveSounds[soundNum].CurrentOffset >= m_SoundList[soundID].SampleCount) {
					m_ActiveSounds[soundNum].CurrentOffset  = 0;

					if (m_ActiveSounds[soundNum].RepeatCount > 1) {
						m_ActiveSounds[soundNum].RepeatCount -= 1;
					}
					else {
						soundID = DWORD(-1);
						m_ActiveSounds[soundNum].SoundID = DWORD(-1);
					}
				}
			}
		}

		short *pDest = m_pSoundBuffer[m_CurrentBuffer];

		// Convert the mixing results back to 16-bit format.  Because of the
		// volume adjustment, all values need to be divided by 256, then
		// clamped to avoid overflow problems that insert severe distortion
		// into the audio playback.
		for (i = 0; i < m_BufferSize; ++i) {
			long sample = m_pMixer[i] >> 8;

			if (sample < -0x7FFF) {
				sample = -0x7FFF;
			}
			else if (sample > 0x7FFF) {
				sample = 0x7FFF;
			}

			pDest[i] = short(sample);
		}
	}

	// After all of that work, we now have a new buffer full of audio data
	// to play back for the next fraction of a second.
	result = waveOutWrite(m_hWaveOut, &m_WaveHeader[m_CurrentBuffer], sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != result) {
		// Something went wrong, queuing can't happen, and all audio playback
		// has probably now stopped since there are no buffers queued.  Need
		// to alert the main rendering thread, so it can reset this class and
		// restart audio playback.
	}

	m_CurrentBuffer = (m_CurrentBuffer + 1) % c_BufferCount;
}


/////////////////////////////////////////////////////////////////////////////
//
//	CallbackHandler()
//
//	This is a static function used by the multimedia library to alert the
//	sound driver that a playback buffer has finished playing.  Hook these
//	messages to immediately refill this buffer and flog it back to the audio
//	driver.
//
//	A more robust implementation would pay attention to all of the parameters,
//	and all of the other message types that could arrive.  All we really need
//	is to know when a buffer is done, and refill it to keep buffers queued at
//	all times.
//
void CALLBACK CwaSoundDriver::CallbackHandler(HWAVEOUT /*hWaveOut*/, UINT message, DWORD /*instance*/, DWORD /*param1*/, DWORD /*param2*/)
{
	if (message == WOM_DONE) {
		if (NULL != g_pSoundDriver) {
			g_pSoundDriver->FillNextBuffer();
		}
	}
}


