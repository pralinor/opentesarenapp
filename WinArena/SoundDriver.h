/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/SoundDriver.h 4     12/23/05 6:10p Lee $
//
//	File: SoundDriver.h
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


#define c_AudioSampleRate	44100


struct SoundInfo_t
{
	DWORD  SampleCount;
	short* pSamples;
};


struct PlayInfo_t
{
	DWORD SoundID;
	DWORD RepeatCount;
	DWORD Volume;
	DWORD CurrentOffset;
};


class CwaSoundDriver
{
public:
	enum {
		c_BufferCount			= 2,
		c_MaxConcurrentSounds	= 8,
		c_MaxSounds				= 128,
	};

	DWORD		m_SampleRate;
	bool		m_IsAudioPlaying;
	HWND		m_hWnd;
	DWORD		m_BufferSize;
	DWORD		m_CurrentBuffer;
	HWAVEOUT	m_hWaveOut;
	WAVEHDR		m_WaveHeader[c_BufferCount];
	short*		m_pSoundBuffer[c_BufferCount];
	long*		m_pMixer;

	DWORD		m_SoundCount;
	SoundInfo_t	m_SoundList[c_MaxSounds];

	PlayInfo_t	m_ActiveSounds[c_MaxConcurrentSounds];


	////////////////////////////////////////////////////////////////////////////

	CwaSoundDriver();
	~CwaSoundDriver();

	bool  Start(void);
	void  Stop(void);
	DWORD AssignSound(short *pSamples, DWORD sampleCount);
	void  Resample(BYTE input[], short output[], DWORD countIn, DWORD countOut, DWORD filterSize);
	void  QueueSound(DWORD id, DWORD repeatCount = 1, DWORD volume = 0x100);
	void  FillNextBuffer(void);

	static void CALLBACK CallbackHandler(HWAVEOUT hWaveOut, UINT message, DWORD instance, DWORD param1, DWORD param2);
};


