/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/World.cpp 12    12/31/06 12:11p Lee $
//
//	File: World.cpp
//
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"
#include "World.h"
#include "TGA.h"
#include <direct.h>
#include <time.h>


#define c_MaxTurnRate	(c_FixedPointOne / 1)		// three seconds to spin in a circle
#define c_MaxMoveRate	(c_FixedPointOne / 1)		// one second to run the length of a tile/block

// These numbers best simulate the field of view from Arena, but introduce
// a prominent fish-eye effect when displaying at lower resolutions.
#define c_FieldOfView			2.2f
#define c_MinCollisionDistance	((c_FixedPointOne * 36) / 100)

/*
#define c_FieldOfView			2.414f
#define c_MinCollisionDistance	((c_FixedPointOne * 34) / 100)
*/
/*
// These numbers give a better view of the world, but the narrow field of
// view makes it difficult to see objects on the floor close to the player.
#define c_FieldOfView			3.000f
#define c_MinCollisionDistance	((c_FixedPointOne * 32) / 100)
*/

#define c_CollisionLoFrac	c_MinCollisionDistance
#define c_CollisionHiFrac	(c_FixedPointOne - c_CollisionLoFrac)

#define c_PerspectiveAdjustLimit	IntToFixed(10)

#define c_Shape_Cell		0x10000000
#define c_Shape_Critter		0x20000000
#define c_Shape_Sprite		0x30000000
#define c_Shape_TypeMask	0xF0000000

#define c_Shape_WallNorth	0x01000000
#define c_Shape_WallEast	0x02000000
#define c_Shape_WallSouth	0x03000000
#define c_Shape_WallWest	0x04000000
#define c_Shape_VoidNorth	0x05000000
#define c_Shape_VoidEast	0x06000000
#define c_Shape_VoidSouth	0x07000000
#define c_Shape_VoidWest	0x08000000
#define c_Shape_Floor		0x09000000
#define c_Shape_Ceiling		0x0A000000





SpellInfo_t g_SpellInfo[] =
{
	{ "Blind",			Casting_Smoke,		Animation_GasBulletLarge,	Animation_GasBurstLarge,	Sound_Explode,	0 },
	{ "Choke",			Casting_Smoke,		Animation_GasBulletSmall,	Animation_GasBurstSmall,	Sound_Burst,	0 },
	{ "Darkness",		Casting_Nothing,	0,							0,							0,				1 },
	{ "Fire Dart",		Casting_Fire,		Animation_FireBulletSmall,	Animation_FireBurstSmall,	Sound_Burst,	0 },
	{ "Fireball",		Casting_Fire,		Animation_FireBulletLarge,	Animation_FireBurstLarge,	Sound_Explode,	0 },
	{ "Fog",			Casting_Nothing,	0,							0,							0,				3 },
	{ "Force Blast",	Casting_Magic,		Animation_MagicBulletLarge,	Animation_MagicBurstLarge,	Sound_Explode,	0 },
	{ "Force Bolt",		Casting_Magic,		Animation_MagicBulletSmall,	Animation_MagicBurstSmall,	Sound_Burst,	0 },
	{ "Fresh Air",		Casting_Nothing,	0,							0,							0,				4 },
	{ "Ice Bolt",		Casting_Ice,		Animation_IceBulletSmall,	Animation_IceBurstSmall,	Sound_Burst,	0 },
	{ "Iceball",		Casting_Ice,		Animation_IceBulletLarge,	Animation_IceBurstLarge,	Sound_Explode,	0 },
	{ "Light",			Casting_Nothing,	0,							0,							0,				2 },
	{ "Poisonbloom",	Casting_Acid,		Animation_AcidBulletLarge,	Animation_AcidBurstLarge,	Sound_Explode,	0 },
	{ "Poisonbolt",		Casting_Acid,		Animation_AcidBulletSmall,	Animation_AcidBurstSmall,	Sound_Burst,	0 },
	{ "Shockball",		Casting_Shock,		Animation_ShockBulletLarge,	Animation_ShockBurstLarge,	Sound_Thunder,	0 },
	{ "Spark Bolt",		Casting_Shock,		Animation_ShockBulletSmall,	Animation_ShockBurstSmall,	Sound_Burst,	0 },
};


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaWorld::CwaWorld(void)
	:	m_pRaster(NULL),
		m_UseFogMap(false),
		m_pCurrentMousePointer(NULL),
		m_MouseMoveEnabled(false),
		m_MouseTurnRate(0),
		m_MouseTurnRateFraction(0),
		m_MouseWalkRate(0),
		m_MouseStrafeRate(0),
		m_AttackState(Attack_Off),
		m_AttackStartTime(0),
		m_AttackAnimating(false),
		m_SpellAnimation(Casting_Nothing),
		m_Direction(0),
		m_DirectionSine(0),
		m_DirectionCosine(c_FixedPointOne),
		m_TurnSpeed(0),
		m_MoveSpeed(0),
		m_StrafeSpeed(0),
		m_Elevation(Elevation_Floor),
		m_MaxDrawDistance(32),
		m_WalkDistance(0),
		m_LeftFootStep(false),
		m_HalfWidth(640),
		m_HalfHeight(400),
		m_ShowFPS(false),
		m_FPS(0),
		m_MouseX(160),
		m_MouseY(100),
		m_RightMouseDown(false),
		m_BaseAttackTime(0),
		m_RandomSoundDelay(10000),
		m_CurrentTime(0),
		m_DisplayMode(Display_Game),
		m_FullBright(false),
		m_PauseState(Pause_NotPaused),
		m_SelectionCount(0),
		m_QuadLimit(0),
		m_QuadCount(0)
{
	srand(DWORD(time(NULL)));

	DWORD weaponNum = rand() % 100;

	if (weaponNum < 16) {
		m_pWeapon = &m_WeaponAxe;
	}
	else if (weaponNum < 33) {
		m_pWeapon = &m_WeaponHammer;
	}
	else if (weaponNum < 50) {
		m_pWeapon = &m_WeaponMace;
	}
	else if (weaponNum < 66) {
		m_pWeapon = &m_WeaponStaff;
	}
	else if (weaponNum < 83) {
		m_pWeapon = &m_WeaponStar;
	}
	else {
		m_pWeapon = &m_WeaponSword;
	}

	memset(m_Sprite, 0, sizeof(m_Sprite));
	memset(m_Critter, 0, sizeof(m_Critter));
	memset(m_Projectile, 0, sizeof(m_Projectile));
	memset(m_Lights, 0, sizeof(m_Lights));
	memset(m_Triggers, 0, sizeof(m_Triggers));
	memset(m_SoundClips, 0, sizeof(m_SoundClips));

	strcpy(m_Character.Name, "Rolf the Uber");
	strcpy(m_Character.Race, "Nord");
	strcpy(m_Character.Class, "Battlemage");
	m_Character.Strength			= 69;
	m_Character.Intelligence		= 42;
	m_Character.Willpower			= 53;
	m_Character.Agility				= 40;
	m_Character.Speed				= 53;
	m_Character.Endurance			= 56;
	m_Character.Personality			= 53;
	m_Character.Luck				= 51;
	m_Character.HealthCurrent		= 37;
	m_Character.HealthMaximum		= 37;
	m_Character.FatigueMaximum		= 125;
	m_Character.FatigueCurrent		= 125;
	m_Character.Gold				= 111;
	m_Character.Experience			= 700;
	m_Character.Level				= 1;
	m_Character.DamageModifier		= 4;
	m_Character.SpellPointsMaximum	= 73;
	m_Character.SpellPointsCurrent	= 73;
	m_Character.MagicDefence		= 0;
	m_Character.ToHitModifier		= 2;
	m_Character.HealthModifier		= 1;
	m_Character.CharismaModifier	= 0;
	m_Character.MaxWeight			= 138;
	m_Character.ArmorClassModifier	= -2;
	m_Character.HealModifier		= 1;

	m_MessageBuffer[0] = '\0';
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaWorld::~CwaWorld(void)
{
	for (DWORD clipNum = 0; clipNum < ArraySize(m_SoundClips); ++clipNum) {
		SafeDeleteArray(m_SoundClips[clipNum].pSamples);
	}

	Unload();
}


#include "giffile.h"


/////////////////////////////////////////////////////////////////////////////
//
//	Load()
//
//	Loads required resources from Global.BSA and specific files in the
//	Arena directory.
//
bool CwaWorld::Load(void)
{
	PROFILE(PFID_World_Load);

	m_Loader.LoadResources();

	g_pArenaLoader = &m_Loader;

	// Load the light maps.  These are needed to fade colors towards either
	// black or fog color, based on the amount of light present.
	m_Loader.LoadLightMap("FOG", m_LightMapFog);
	m_Loader.LoadLightMap("NORMAL", m_LightMapNormal);

//	HexDump("DITHER.IMG");
//	m_Loader.ExtractFile("OVERSNOW.XFM");
//	HexDump("MENU.IMG");
//	ScanIMGs();
/*
	{
		char *a = new char[64000];
		char *b = new char[64000];
		FILE *p = fopen("face2.tga", "rb");
		fseek(p, 18+768, SEEK_SET);
		fread(a, 1, 64000, p);
		fclose(p);
		p = fopen("boo.tga", "rb");
		fseek(p,  18+768, SEEK_SET);
		fread(b, 1, 64000, p);
		fclose(p);

		CGIFFile file;

		GIFFrameBuffer frame;
		frame.Width      = 320;
		frame.Height     = 200;
		frame.Pitch      = 320;
		frame.BufferSize = 320 * 200;
		frame.pBuffer    = (BYTE*)a;
		frame.ColorMapEntryCount = 256;

		BYTE pal[3*256];
		for (DWORD i = 0; i < 256; ++i) {
			pal[i*3  ] = BYTE(g_PaletteCharSheet[i] >> 16);
			pal[i*3+1] = BYTE(g_PaletteCharSheet[i] >> 8);
			pal[i*3+2] = BYTE(g_PaletteCharSheet[i]);
		}

		// Use the raw palette.  It's still in 24-bit format, and has the
		// bass ackwards byte ordering loved by GIF.
		memcpy(frame.ColorMap, pal, 3 * 256);

		file.Open("coffee.gif", false);
		file.SetCodeSize(8);
		file.SetGlobalColorMap(256, pal);
		file.SetDelayTime(100);
		file.InitOutput(320, 200);
		file.StartAnimation();
		file.WriteImage(frame);
		file.SetDelayTime(5);
		frame.pBuffer    = (BYTE*)b;
		file.WriteImage(frame);

		file.FinishOutput();
		file.Close();
	}
*/
/*
	{
//		FILE *p = fopen("H:\\Arena\\TEMPSAVE.$$$", "rb");
//		FILE *d = fopen("dump.txt", "w");
		FILE *p = fopen("democity.dat", "rb");
		FILE *d = fopen("dumpcity.txt", "w");
		BYTE buffer[256];
		for (DWORD i = 0; i < 256+128; ++i) {
			fread(buffer, 1, 256, p);
			for (DWORD j = 0; j < 256; j+=2) {
				fprintf(d, "%02X%02X ", buffer[j+1], buffer[j]);
			}
			fprintf(d, "\n");
		}
	}
*/
	// Load some of the wall textures for rendering.  Stick them at known
	// positions in the array so we can more easily reference them when
	// creating test levels.
	m_TextureList[Texture_TrenchCeiling	].LoadWallTexture("WALLF.SET",    2, false);
	m_TextureList[Texture_Abyss			].LoadWallTexture("IWATER.IMG",   0, false);

	m_TextureList[Texture_ChestClosed	].LoadIMG("CHESTC.IMG",   true);
	m_TextureList[Texture_ChestOpen		].LoadIMG("CHESTO.IMG",   true);
	m_TextureList[Texture_Equipment		].LoadIMG("EQUIPMEN.IMG", true);
	m_TextureList[Texture_Treasure1		].LoadIMG("TREASRE1.IMG", true);
	m_TextureList[Texture_Treasure2		].LoadIMG("TREASRE2.IMG", true);
	m_TextureList[Texture_Treasure3		].LoadIMG("TREASRE3.IMG", true);

	// HUD = "P1.IMG"  320 x 53
	// or POPTALK.IMG 320 x 77
	// or S2, 320 x 36
	// compass is SLIDER.IMG, 288 x 7
	//
	// NOTE: S2.IMG and POPTALK.IMG are alternate HUDs, possibly related
	// to the early plans for Arena to be party-based.  From POPTALK.IMG,
	// it looks like they may have been considering networking support also.
	m_TextureList[Texture_HUD			].LoadIMG("P1.IMG",       true);
	m_TextureList[Texture_Pointer		].LoadIMG("ARENARW.IMG",  true);
	m_TextureList[Texture_CompassBorder	].LoadIMG("COMPASS.IMG",  true);
	m_TextureList[Texture_CompassLines	].LoadIMG("SLIDER.IMG",   true);

	m_TextureList[Texture_Automap		].LoadIMG("AUTOMAP.IMG",  true);
	m_TextureList[Texture_EquipmentPopup].LoadIMG("NEWPOP.IMG",   true);
	m_TextureList[Texture_ControlPanel	].LoadIMG("OP.IMG",       true);
	m_TextureList[Texture_Journal		].LoadIMG("LOGBOOK.IMG",  true);
	m_TextureList[Texture_LoadSave		].LoadIMG("LOADSAVE.IMG", true);
//	m_TextureList[Texture_SpellBook		].LoadIMG("CHARSPEL.IMG", true);
	m_TextureList[Texture_SpellPage		].LoadIMG("CHARSPEL.IMG", true);

	m_Water.LoadWater();

	m_MousePointer.LoadCIF("ARROWS.CIF", 9);

	m_WeaponAxe.LoadCIF("AXE.CIF", 33);
	m_WeaponHammer.LoadCIF("HAMMER.CIF", 33);
	m_WeaponHand.LoadCIF("HAND.CIF", 13);
	m_WeaponMace.LoadCIF("MACE.CIF", 33);
	m_WeaponStaff.LoadCIF("STAFF.CIF", 33);
	m_WeaponStar.LoadCIF("STAR.CIF", 33);
	m_WeaponSword.LoadCIF("SWORD.CIF", 33);

	m_SpellAcid.LoadAnimation("ACID.CFA");
	m_SpellFire.LoadAnimation("FIRE.CFA");
	m_SpellIce.LoadAnimation("ICE.CFA");
	m_SpellMagic.LoadAnimation("MAGIC.CFA");
	m_SpellShock.LoadAnimation("SHOCK.CFA");
	m_SpellSmoke.LoadAnimation("SMOKE.CFA");

	for (DWORD animNum = 0; animNum < Animation_ArraySize; ++animNum) {
		DWORD id = g_AnimInfo[animNum].ID;

		m_Animations[id].LoadAnimation(g_AnimInfo[animNum].Name);
	}

	// Load up the resampled audio data.  These files are stored as WAVs in
	// the Cache directory.  If those files do not yet exist, display a dialog
	// box to warn the user that the files are about the be created, then
	// process the audio files and save them to the Cache.  This will take at
	// least 20 seconds for a fast system, longer for low-end systems.

	// Use a flag so we only warn the user once if a file is missing from the
	// Cache directory.
	bool directoryWarning = false;

	for (DWORD clipNum = 0; clipNum < Sound_ArraySize; ++clipNum) {
		PROFILE(PFID_Load_Sound);

		// If for some reason resources were already loaded, don't reload
		// the audio data.
		if (NULL != m_SoundClips[clipNum].pSamples) {
			continue;
		}

		SoundFile_t &info = g_SoundFile[clipNum];

		char filename[256];
		sprintf(filename, "%sCache\\%sres.wav", g_ArenaPath, info.Name);

		// The normal case is that the file already exists in the cache.
		// This allows us to read the resampled audio from disk.
		FILE *pFile = fopen(filename, "rb");
		if (NULL != pFile) {
			DWORD sampleCount = 0;

			// FIXME: add error checking to verify this is a 16-bit, 44.1K
			// wave file

			fseek(pFile, 42, SEEK_SET);
			fread(&sampleCount, sizeof(DWORD), 1, pFile);

			// The value read for the same count is actually a byte count.
			// Need to divide by 2 to get samples, since the audio data is
			// stored at 16-bit data.
			sampleCount /= 2;

			m_SoundClips[clipNum].pSamples = new short[sampleCount];

			fread(m_SoundClips[clipNum].pSamples, sizeof(short), sampleCount, pFile);
			fclose(pFile);

			m_SoundClips[clipNum].SampleCount = sampleCount;
		}

		// The other case should only arise the first time the program runs.
		// The resampled sound files don't exist, so it will need to do all
		// of the resampling and filtering to obtain a cleaner, 16-bit 44.1K
		// sound clip.  Since the resampling code uses sin(x)/x for the
		// processing, this is very slow.
		else {
			char resourceName[64];
			sprintf(resourceName, "%s.VOC", info.Name);

			// Read the original audio data into a buffer.  The buffer
			// returned was dynamically allocated, so we need to free it
			// when we're done with it.  The data in the buffer will be
			// raw PCM data without any headers.
			DWORD oldSampleCount, oldSampleRate;
			BYTE* p8bitData = m_Loader.LoadSound(resourceName, oldSampleCount, oldSampleRate);

			// If this sound clip has garbage at the start of the buffer,
			// replace those samples with 0x80, the 8-bit value representing
			// silence.
			if (g_SoundFile[clipNum].TrimStart > 0) {
				memset(p8bitData, 0x80, g_SoundFile[clipNum].TrimStart);
			}

			// How many samples will there be after the data is resampled up
			// to 44.1K?  This pads 8 samples of silence before and after
			// the input data to help ramp towards silence.  This is needed
			// to compensate for some of the clips that do not always start
			// or end with silence.
			DWORD sampleCount = (c_AudioSampleRate * (16 + oldSampleCount)) / oldSampleRate;

			m_SoundClips[clipNum].SampleCount = sampleCount;
			m_SoundClips[clipNum].pSamples    = new short[sampleCount];

			ResampleAudio(p8bitData, m_SoundClips[clipNum].pSamples, oldSampleCount, sampleCount, g_SoundFile[clipNum].FilterLength);

			// Discard the original 8-bit data.  We'll only use the resampled
			// 16-bit data from here on out.
			SafeDeleteArray(p8bitData);

			// The first time we have to do the resampling, warn the user
			// to expect a delay, and make certain that the cache directory
			// actually exists so we can write to it.
			if (false == directoryWarning) {
				directoryWarning = true;

				MessageBox(NULL,	"No data cache detected.\n"
									"Cache will be built after you close this box.\n"
									"This may take up to a minute.", "Warning", MB_OK | MB_ICONEXCLAMATION);

				MakeArenaDirectory("Cache");
			}

			// Save the resampled audio off to a wave file so it won't need
			// to be resampled the next time the program starts up.
			FILE *pFile = fopen(filename, "wb");
			if (NULL != pFile) {
				WAVEFORMATEX waveHeader;
				waveHeader.wFormatTag      = 1;
				waveHeader.nChannels       = 1;
				waveHeader.nSamplesPerSec  = c_AudioSampleRate;
				waveHeader.nAvgBytesPerSec = sizeof(short) * c_AudioSampleRate;
				waveHeader.nBlockAlign     = 2;
				waveHeader.wBitsPerSample  = 16;
				waveHeader.cbSize          = 0;

				long headerSize = sizeof(waveHeader);

				// Add 2 words for WAVE chunk, 1 for format tag, and 2 for data chunk.  The
				// initial 2 words for the RIFF chunk are not counted as part of the size.
				long fileSize = sizeof(short) * sampleCount + headerSize + (5 * sizeof(DWORD));

				long chunkSize = sampleCount * 2;

				fwrite("RIFF", 4, 1, pFile);				// Write "RIFF"
				fwrite(&fileSize, 4, 1, pFile);				// Write Size of file with header, not counting first 8 bytes
				fwrite("WAVE", 4, 1, pFile);				// Write "WAVE"
				fwrite("fmt ", 4, 1, pFile);				// Write "fmt "
				fwrite(&headerSize, 4, 1, pFile);			// Size of header
				fwrite(&waveHeader, 1, headerSize, pFile);
				fwrite("data", 4, 1, pFile);				// Write "data"
				fwrite(&chunkSize, sizeof(DWORD), 1, pFile);
				fwrite(m_SoundClips[clipNum].pSamples, sizeof(short), sampleCount, pFile);
				fclose(pFile);
			}
		}

		// Hand the 16-bit, 44.1K audio to the sound driver, and assign it
		// a known ID value so we can easily reference the sound clips.
		m_SoundClips[info.ID].SoundID = m_SoundDriver.AssignSound(m_SoundClips[clipNum].pSamples, m_SoundClips[clipNum].SampleCount);
	}

	for (DWORD songNum = 0; songNum < Song_ArraySize; ++songNum) {
		DWORD songSize = 0;
		BYTE* pSong    = g_pArenaLoader->LoadData(g_SongFile[songNum].Name, songSize);

		m_SongList[songNum].SetBuffer(pSong, songSize, false);
	}

//	BuildTestLevel();
//	BuildDemoDungeon("DemoCity.dat", "tcn.inf", 88, 88, true);
	BuildDemoDungeon("DemoDungeon.dat", "start.inf", 50, 50, false);
//	BuildTestCity();

	m_MidiEvent.EventNumber = 0;
	m_MidiEvent.FullRepeat  = FALSE;
	m_MidiEvent.RepeatCount = 255;
	m_MidiPlayer.StartThread();
	m_MidiPlayer.QueueSong(&m_SongList[Song_Dungeon1], &m_MidiEvent, 1, true);

	m_SoundDriver.Start();

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	Unload()
//
void CwaWorld::Unload(void)
{
	PROFILE(PFID_World_Unload);

	m_MidiPlayer.StopThread();
	m_SoundDriver.Stop();

	g_pArenaLoader = NULL;

	m_Loader.FreeResources();
}


/////////////////////////////////////////////////////////////////////////////
//
//	ResampleAudio()
//
//	Does a sin(x)/x resampling of audio data.  The input is 8-bit audio from
//	Global.BSA, which may be sampled anywhere from 5K to 11K.  The output
//	is 16-bit audio at 44.1K (or whatever c_AudioSampleRate is currently set
//	to be).
//
//	The resampling will also do a very poor low-pass filtering by sequentially
//	averaging samples together, as controlled by <filterSize>.  There are
//	better ways of doing this, but this will eliminate the hissing due to
//	8-bit quantization.  The large increase in sample rate and going from
//	8- to 16-bit causes sharp-edged plateaus in the waveform.  In other words,
//	the resulting audio data is full of square waves.  Performing a sequential
//	after smooths the rise/fall of the square waves and eliminates much of
//	the high-frequency hissing.  It also muddies the sound.  A specific
//	<filterSize> was picked for each sound clip to get a balance between
//	eliminating high-frequency noise without muddying up the sound too much.
//
//	A full-blown DSP FFT algorithm might perform better, but frankly if you
//	want to clean up the WAVs, you're much better off loading the originals
//	into CoolEdit or some other app and doing it right.
//
void CwaWorld::ResampleAudio(BYTE input[], short output[], DWORD countIn, DWORD countOut, DWORD filterSize)
{
	float angle, amp;

	// Keep track of the most recent <filterSize> samples.
	float filter[32];
	memset(filter, 0, sizeof(filter));

	for (long sampleNum = 0; sampleNum < long(countOut); ++sampleNum) {

		float middle    = float(sampleNum) * float(countIn) / float(countOut);
		long  baseIndex = long(middle);
		float fraction  = middle - float(baseIndex);

		float accum = 0.0f;

		for (long delta = -13; delta <= 13; ++delta) {

			// Map this sample back to an input value.  Apply a -8 shift on
			// the input to apply extra silence samples to the beginning and
			// end of the clip.
			long i = baseIndex + delta - 8;

			long sample = 0;

			// Only read samples from within the bounds of the input buffer.
			// Samples off the front or back of the buffer will default to
			// silence.
			if (DWORD(i) < countIn) {
				sample = long(input[i]) - 128;
			}


			angle = 3.14159f * (float(delta) - fraction);

			// Need special case for samples falling exactly on an input
			// value, otherwise we'll have div-zero errors from sin(0)/0.
			if (fabsf(angle) < 0.01f) {
				amp = 1.0f;
			}
			else {
				amp = sinf(angle) / angle;
			}

			accum += float(sample) * amp;
		}

		// Some of the sounds clip min/max range.  Scale the audio to 7/8
		// volume so the resampled data won't clip.
		accum *= float(256 - 32);

		// When filtering via sequential averaging, keep a running list
		// of the last <filterSize> samples and average the together to
		// produce the low-pass filtered result.
		if (filterSize > 1) {
			filter[sampleNum % filterSize] = accum;

			accum = filter[0];
			for (DWORD step = 1; step < filterSize; ++step) {
				accum += filter[step];
			}

			accum /= float(filterSize);
		}

		// Convert float to int, using biasing to properly round.
		long bits = (accum < 0.0f) ? long(accum - 0.5f) : long(accum + 0.5f);

		// Clamp to legal 16-bit signed values.
		if (bits < -0x7FFF) {
			output[sampleNum] = -0x7FFF;
		}
		else if (bits > 0x7FFF) {
			output[sampleNum] = 0x7FFF;
		}
		else {
			output[sampleNum] = short(bits);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpMusic()
//
//	Dumps all of the MIDI tracks out to files in the standard .mid file
//	format (instead of the obsolete XMIDI format used in Arena's resource
//	files).
//
void CwaWorld::DumpMusic(void)
{
	MakeArenaDirectory("Music");

	for (DWORD songNum = 0; songNum < Song_ArraySize; ++songNum) {
		char filename[256];
		sprintf(filename, "%sMusic\\%s", g_ArenaPath, g_SongFile[songNum].MidiName);

		DWORD volume = 100;

		// Some tracks are much louder than others.  Scale back their volume
		// to make it more in line with the other tracks.
		if ((songNum == DWORD(Song_ArabVillage)) ||
			(songNum == DWORD(Song_ArabCity)) ||
			(songNum == DWORD(Song_ArabTown)) ||
			(songNum == DWORD(Song_City)) ||
			(songNum == DWORD(Song_Equipment)) ||
			(songNum == DWORD(Song_Square)) ||
			(songNum == DWORD(Song_Tavern)))
		{
			volume = 75;
		}
		if (songNum == DWORD(Song_Palace)) {
			volume = 65;
		}

		m_SongList[songNum].ExportXmidi(filename, volume);


		// Test code: this will parse the MIDI codes, writing them out into
		// text format.  The song's filename will be extended to have ".txt"
		// as the file extension, so "song.mid" gets parsed into "song.mid.txt".
//		m_MidiPlayer.ParseMidiFile(filename);

/*
		DWORD songSize = 0;
		BYTE* pSong    = g_pArenaLoader->LoadData(g_SongFile[songNum].Name, songSize);

		char filename[256];
		sprintf(filename, "a_%s.log", g_SongFile[songNum].MidiName);
		FILE *pLog  = fopen(filename, "w");
		sprintf(filename, "a_%s", g_SongFile[songNum].MidiName);
		FILE *pMidi = fopen(filename, "wb");

		DWORD trackNumber = 0;
		m_MidiPlayer.ParseXmidiData(pSong, songSize, 0, trackNumber, pLog, pMidi);

		fclose(pLog);
		fclose(pMidi);
*/
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ShowFPS()
//
//	Accessor function to enable/disable displaying FPS in upper left corner.
//
void CwaWorld::ShowFPS(bool enable)
{
	m_ShowFPS = enable;
}


/////////////////////////////////////////////////////////////////////////////
//
//	EnablePlayerIllumination()
//
//	Accessor function to control whether a light source is present at the
//	player's location.  In Arena, this is always the case.  Allow this to
//	be disabled for easy testing of lighting model in renderer.
//
void CwaWorld::EnablePlayerIllumination(bool enable)
{
	m_IlluminatePlayer = enable;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetFPS()
//
//	Assuming the FPS display is enabled, this sets the value that will be
//	displayed there.
//
void CwaWorld::SetFPS(DWORD fps)
{
	m_FPS = fps;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetResolution()
//
//	Sets the resolution used to render the 3D display.  Width is ignored
//	since the only supported resolutions are 320x200, 640x400, and 960x600.
//
void CwaWorld::SetResolution(DWORD /*width*/, DWORD height)
{
	// Set the half width/height values.  These are stored as half, since
	// coordinate transforms are relative to the center of the screen.
	if (height < 400) {
		m_HalfWidth  = 320 / 2;
		m_HalfHeight = 146 / 2;
	}
	else if (height < 600) {
		m_HalfWidth  = 320;
		m_HalfHeight = 146;
	}
	else {
		m_HalfWidth  = 320 + 160;
		m_HalfHeight = 146 +  73;
	}

	m_FieldOfViewX = long(c_FieldOfView * float(c_FixedPointOne) * float(m_HalfHeight) / float(m_HalfWidth));
	m_FieldOfViewY = long(c_FieldOfView * float(c_FixedPointOne));

	m_InverseFieldOfViewX = FixedDiv(c_FixedPointOne, m_FieldOfViewX);
	m_InverseFieldOfViewY = FixedDiv(c_FixedPointOne, m_FieldOfViewY);
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetRaster()
//
//	Accessor function used to assign a rasterizer to use for rendering.
//
void CwaWorld::SetRaster(CwaRaster *pRaster)
{
	m_pRaster = pRaster;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetRenderDistance()
//
//	Specifies the maximum distance at which cells will be drawn.  Cells
//	farther away will not be drawn (typically creating artifacts on the
//	horizon).
//
void CwaWorld::SetRenderDistance(DWORD distance)
{
	m_MaxDrawDistance = distance;
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScrollWheel()
//
//	This is called if a mouse scroll wheel is turned.  Obviously not supported
//	in the original Arena, this does make some menu accesses easier.
//
void CwaWorld::ScrollWheel(bool down)
{
	if (Display_Game == m_DisplayMode) {
		if (0 != m_SelectionCount) {
			if (down) {
				ScrollSelectMenuDown();
			}
			else {
				ScrollSelectMenuUp();
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScrollSelectMenuUp()
//
void CwaWorld::ScrollSelectMenuUp(void)
{
	if (m_SelectionOffset > 0) {
		--m_SelectionOffset;
		if (m_SelectionIndex > (m_SelectionOffset + 4)) {
			m_SelectionIndex = m_SelectionOffset + 4;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScrollSelectMenuDown()
//
void CwaWorld::ScrollSelectMenuDown(void)
{
	if ((m_SelectionOffset + m_SelectionMaxVisi) < m_SelectionCount) {
		++m_SelectionOffset;

		if (m_SelectionIndex < m_SelectionOffset) {
			m_SelectionIndex = m_SelectionOffset;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetMousePosition()
//
//	Specifies the current mouse position in raster coordinates.  If the size
//	of the window is different, these coordinates need to have already been
//	scaled to raster coordinates.  This code knows nothing of the window
//	size, so it cannot do that transform itself.
//
void CwaWorld::SetMousePosition(DWORD x, DWORD y)
{
	m_MouseX = x;
	m_MouseY = y;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LeftMouseClick()
//
//	The user clicked the left mouse button.  Depending upon what display
//	mode the screen is in, figure out what was clicked and whether anything
//	should occur as a result of that click.
//
bool CwaWorld::LeftMouseClick(void)
{
	DWORD scale = (m_pRaster->FrameHeight() / 200);

	// Convert the mouse coords to 320x200 units.  This simplifies coordinate
	// tests, since it is easy to check the source graphics to determine what
	// was clicked, and those graphics are all at the 320x200 resolution.
	DWORD x = m_MouseX / scale;
	DWORD y = m_MouseY / scale;

	switch (m_DisplayMode) {
		case Display_Automap:
			// The "Exit" button in lower right corner switches back to the
			// normal 3D game mode.
			if ((242 <= x) && (x <= 282) && (164 <= y) && (y <= 180)) {
				m_DisplayMode = Display_Game;
			}
			break;

		case Display_Character:
			// The "Done" button in lower left corner returns back to game mode.
			if ((17 <= x) && (x <= 33) && (181 <= y) && (y <= 188)) {
				m_DisplayMode = Display_Game;
			}

			// The "Next" button in lower right corner switches to the
			// inventory screen.
			if ((114 <= x) && (x <= 148) && (181 <= y) && (y <= 188)) {
				m_DisplayMode = Display_Inventory;
			}
			break;

		case Display_ControlPanel:

			// User clicked along the row of buttons at the bottom of the
			// control screen.
			if ((120 <= y) && (y <= 144)) {

				// New Game
				if ((3 <= x) && (x <= 60)) {
				}

				// Load Game
				else if ((68 <= x) && (x <= 124)) {
					m_DisplayMode = Display_LoadSave;
				}

				// Save Game
				else if ((132 <= x) && (x <= 188)) {
					m_DisplayMode = Display_LoadSave;
				}

				// Drop to DOS
				else if ((186 <= x) && (x <= 252)) {
					PostQuitMessage(0);
				}

				// Continue
				else if ((260 <= x) && (x <= 317)) {
					m_DisplayMode = Display_Game;
				}
			}
			break;

		case Display_Game:
			if (Pause_SelectLoot == m_PauseState) {
				LeftMouseClickLootPile(x, y);

				// Return false so clicking on something doesn't cause the
				// character to turn or move after clicking on the box for
				// the final time, making it disappear.
				return false;
			}

			// User clicked in the center of the 3D area.  Do a picking
			// operation to see if there is something clickable at that
			// point on the screen (doors, NPCs, loot, etc.).
			if ((y < 147) && (false == m_MouseMoveEnabled)) {
				// Hot spot for mouse cursor is upper left corner.  Adjust the
				// sample point down and over 5 pixels so the sampled position
				// is in the middle of the crosshairs.
				long sampleX = m_MouseX + (5 * scale);
				long sampleY = m_MouseY + (5 * scale);

				// Clamp to safe limits.  The sample point should be near the
				// center of the screen, but be safe just in case.
				if (sampleX >= (2 * m_HalfWidth)) {
					sampleX  = (2 * m_HalfWidth) - 1;
				}

				if (sampleY >= (2 * m_HalfHeight)) {
					sampleY  = (2 * m_HalfHeight) - 1;
				}

				// First check the depth of that point.  If it is too far
				// away, do not allow picking.  Need to allow for the z
				// bias used to increase precision in the depth buffer,
				// and use a greater-than comparison since the depth buffer
				// stores 1/z.
				RenderQuadListWithDepth();
				if (m_pRaster->GetPixelDepth(sampleX, sampleY) > (c_FixedPointOne )) {//<< c_ZDepthBias)) {
					DWORD pointID = m_pRaster->GetShapeID(sampleX, sampleY);

					// If we end up with a non-zero value, something
					// interactive was clicked.
					if (0 != pointID) {
						ClickOnID(pointID);
					}
				}
			}

			// Face == character screen
			if (( 14 <= x) && (x <=  53) && (166 <= y) && (y <= 194)) {
				m_DisplayMode = Display_Character;
			}

			// Attack button toggles attack mode on or off.
			if (( 89 <= x) && (x <= 116) && (151 <= y) && (y <= 171)) {
				if (Attack_Off == m_AttackState) {
					m_AttackState     = Attack_Idle;
					m_BaseAttackTime  = m_CurrentTime;
					m_AttackAnimating = true;
				}
				else {
					m_AttackState     = Attack_Off;
					m_BaseAttackTime  = m_CurrentTime;
					m_AttackAnimating = true;
				}
			}

			// Automap
			if ((119 <= x) && (x <= 145) && (151 <= y) && (y <= 171)) {
				m_DisplayMode = Display_Automap;
			}

			// Skullduggery
			if ((148 <= x) && (x <= 175) && (151 <= y) && (y <= 171)) {
			}

			// Health
			if ((178 <= x) && (x <= 205) && (151 <= y) && (y <= 171)) {
			}

			// Spell casting
			if (( 89 <= x) && (x <= 116) && (175 <= y) && (y <= 195)) {
				// If already paused to select a spell, unpause.
				if (Pause_SelectSpell == m_PauseState) {
					m_PauseState = Pause_NotPaused;
				}

				// Otherwise pause until the player selects a spell.
				else {
					m_PauseState = Pause_SelectSpell;

					// Need to init the selection code with a list of spells
					// that could be selected.
					m_SelectionCompleted = false;
					m_SelectionCount     = ArraySize(g_SpellInfo);
					m_SelectionMaxVisi   = 5;
					m_SelectionIndex     = 0;
					m_SelectionOffset    = 0;

					for (DWORD i = 0; i < ArraySize(g_SpellInfo); ++i) {
						m_SelectionList[i] = g_SpellInfo[i].Name;
					}
				}
			}

			// Journal
			if ((119 <= x) && (x <= 145) && (175 <= y) && (y <= 195)) {
				m_DisplayMode = Display_Journal;
			}

			// Use Item
			if ((148 <= x) && (x <= 175) && (175 <= y) && (y <= 195)) {
				// If already paused to select an item, unpause.
				if (Pause_SelectItem == m_PauseState) {
					m_PauseState = Pause_NotPaused;
				}

				// Otherwise pause until the player selects an item.
				else {
					m_PauseState = Pause_SelectItem;

					// Need to init selection code with a list of items that
					// can be used.
					m_SelectionCompleted = false;
					m_SelectionCount     = 2;
					m_SelectionMaxVisi   = 5;
					m_SelectionIndex     = 0;
					m_SelectionOffset    = 0;
					m_SelectionList[0]   = "Mark of Sharpie";
					m_SelectionList[1]   = "Necrophile's Amulet";
				}
			}

			// Camp
			if ((178 <= x) && (x <= 205) && (175 <= y) && (y <= 195)) {
			}

			// If in selection mode, there will be some items displayed in the
			// list in lower-right corner of the screen.  Respond to clicks
			// there to scroll through the list, and to click on items to select
			// them.  Clicking on the selected item "actives" it, which will be
			// dealt with the next time the world is updated.
			//
			if (0 != m_SelectionCount) {

				// FIXME: add support for dragging the list

				// Scroll-up button.  If we're not already at the bottom of
				// the list, scroll the list down one slot by decrementing the
				// offset into the list.
				if ((208 <= x) && (x <= 216) && (150 <= y) && (y <= 158)) {
					ScrollSelectMenuUp();
				}

				// Scroll-down button.  If not at the bottom of the list, move
				// down the list, causing things to appear to shift upwards.
				if ((208 <= x) && (x <= 216) && (189 <= y) && (y <= 197)) {
					ScrollSelectMenuDown();
				}

				// Player clicked on one of the menu items.  If the item was
				// not already selected, it becomes selected.  If the item was
				// already selected, this will active the item.  The result of
				// selecting something will be dealt with the next time that
				// AdvanceTime() is called.
				//
				if ((222 <= x) && (x <= 302) && (153 <= y) && (y <= 192)) {
					DWORD index = (y - 153) / 8;

					if ((m_SelectionOffset + index) < m_SelectionCount) {
						if (m_SelectionIndex != (m_SelectionOffset + index)) {
							m_SelectionIndex = m_SelectionOffset + index;
						}
						else {
							m_SelectionCompleted = true;
						}
					}
				}
			}

			break;

		case Display_Inventory:
			// Exit
			if ((0 <= x) && (x <= 46) && (188 <= y) && (y <= 199)) {
				m_DisplayMode = Display_Character;
			}

			// Spellbook
			if ((48 <= x) && (x <= 122) && (188 <= y) && (y <= 199)) {
				m_DisplayMode = Display_SpellBook;
			}

			// Drop
			if ((124 <= x) && (x <= 169) && (188 <= y) && (y <= 199)) {
			}
			break;

		case Display_Journal:
			if ((262 <= x) && (x <= 298) && (178 <= y) && (y <= 194)) {
				m_DisplayMode = Display_Game;
			}
			break;

		case Display_LoadSave:
			break;

		case Display_SpellBook:

			// Exit
			if ((0 <= x) && (x <= 46) && (188 <= y) && (y <= 199)) {
				m_DisplayMode = Display_Character;
			}

			// Equipment
			if ((48 <= x) && (x <= 122) && (188 <= y) && (y <= 199)) {
				m_DisplayMode = Display_Inventory;
			}

			// Drop
			if ((124 <= x) && (x <= 169) && (188 <= y) && (y <= 199)) {
			}
			break;

		case Display_SpellPage:

			// Next
			if ((13 <= x) && (x <= 60) && (187 <= y) && (y <= 197)) {
			}

			// Previous
			if ((105 <= x) && (x <= 171) && (187 <= y) && (y <= 197)) {
			}

			// Exit
			if ((288 <= x) && (x <= 306) && (187 <= y) && (y <= 197)) {
				m_DisplayMode = Display_SpellBook;
			}
			break;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	RightMouseClick()
//
void CwaWorld::LeftMouseClickLootPile(DWORD x, DWORD y)
{
	long dx = long(x) - 84;
	long dy = long(y) - 34;

	if ((x >= 65) && (x <= 73) && (y >= 19) && (y <= 27)) {
		if (m_SelectionIndex > 0) {
			m_SelectionIndex -= 1;
			if (m_SelectionOffset > m_SelectionIndex) {
				m_SelectionOffset = m_SelectionIndex;
			}
		}
	}
	else if ((x >= 65) && (x <= 73) && (y >= 92) && (y <= 100)) {
		if ((m_SelectionIndex + 1) < m_SelectionCount) {
			m_SelectionIndex += 1;
			if (m_SelectionIndex > (m_SelectionOffset + m_SelectionMaxVisi - 1)) {
				m_SelectionOffset = m_SelectionIndex - (m_SelectionMaxVisi - 1);
			}
		}
	}
	else if ((DWORD(dx) < 150) && (DWORD(dy) < 56)) {
		DWORD index = (dy / 8) + m_SelectionOffset;
		if (index < m_SelectionCount) {
			// Clicking on a highlighted item moves it to the player's inventory.
			if (index == m_SelectionIndex) {
				for (DWORD i = index; (i+1) < m_SelectionCount; ++i) {
					m_SelectionList[i] = m_SelectionList[i+1];
				}
				m_SelectionCount -= 1;

				// When the last item is removed, close this screen and destroy
				// the loot pile.
				if (0 == m_SelectionCount) {
					m_PauseState = Pause_NotPaused;

					// If this is a loot file (and not a corpse), free the sprite.
					if (SpriteFlag_Loot & m_Sprite[m_SelectionID & 0xFFF].Flags) {
						FreeSprite(m_SelectionID & 0xFFF);
					}
				}
			}

			// Clicking on an un-highlighted item highlights it.
			else {
				m_SelectionIndex = index;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RightMouseClick()
//
//	The user right-clicked with the mouse.  In Arena, this only seems to
//	be used to trigger attack animations.
//
void CwaWorld::RightMouseClick(bool down)
{
	if (Display_Game == m_DisplayMode) {

		// FIXME: is this true for Arena if the user right clicks on the
		// HUD, or only in the 3D window?

		if (down) {
			m_RightMouseDown = true;

			if (Pause_SelectLoot == m_PauseState) {
				m_PauseState = Pause_NotPaused;
			}
			else if (false == m_AttackAnimating) {
				m_BaseAttackTime = m_CurrentTime;
				m_RightMouseX    = m_MouseX;
				m_RightMouseY    = m_MouseY;
			}
		}
		else {
			m_RightMouseDown = false;
		}
	}
}


DWORD g_RandomSoundList[] =
{
	Sound_Clicks,
	Sound_Drip1,
	Sound_Drip2,
//	Sound_Eerie,
//	Sound_HumEerie,
	Sound_Squish,
	Sound_Wind,
};


/////////////////////////////////////////////////////////////////////////////
//
//	AdvanceTime()
//
//	Advances time by some number of milliseconds.  This will update the state
//	of the world, but not result in any rendering on the screen.
//
void CwaWorld::AdvanceTime(DWORD delta)
{
	// Only advance world time if the game is not in a paused state.  The
	// game is considered to be paused when displaying any screen other than
	// the 3D world, or while interacting with a UI element (selecting a spell
	// from a list, accessing inventory of a loot object, dialog, etc.).
	if ((Pause_NotPaused == m_PauseState) && (Display_Game == m_DisplayMode)) {
		m_CurrentTime += delta;

		MovePlayer(delta);

		// Check the delay until the next random environment sound.  If it
		// has been long enough, pick a random sound from the list and reset
		// the delay until the next sound.
		if (m_RandomSoundDelay <= delta) {
			DWORD soundID = g_RandomSoundList[rand() % ArraySize(g_RandomSoundList)];
			DWORD volume  = (rand() % 128) + 64;

			m_SoundDriver.QueueSound(soundID, 1, volume);

			m_RandomSoundDelay = ((rand() % 10) + 10) * 1000;
		}
		else {
			m_RandomSoundDelay -= delta;
		}

		m_CurrentTimeDelta = delta;
	}
	else {
		m_CurrentTimeDelta = 0;
	}

	// User selected something from a list in the lower left corner of the
	// screen.  This could be a spell, or a usable item.
	if (m_SelectionCompleted) {
		if (Pause_SelectSpell == m_PauseState) {
			m_SelectionCompleted = false;
			m_PauseState         = Pause_NotPaused;

			// FIXME: targetted spells need to allow clicking on the screen to
			// fire off the spell, rather than doing it immediately

			// A spell has been selected.  Trigger the casting of the spell.
			if (m_SelectionIndex < ArraySize(g_SpellInfo)) {
				m_SpellAnimation = CastingSpell_t(g_SpellInfo[m_SelectionIndex].AnimationID);

				// If this spell fires a projectile, need to trigger both
				// a casting animation and a projectile with the appropriate
				// transient animation.
				if (0 != g_SpellInfo[m_SelectionIndex].BulletID) {
					DWORD projNum = 0;

					// Look for a projectile slot that is not in use.
					for (DWORD i = 0; i < ArraySize(m_Projectile); ++i) {
						if (0 == m_Projectile[i].Active) {
							projNum = i;
							break;
						}
					}

					m_Projectile[projNum].DeltaX		=  m_DirectionSine   * 2;
					m_Projectile[projNum].DeltaZ		= -m_DirectionCosine * 2;
					m_Projectile[projNum].BulletID		= g_SpellInfo[m_SelectionIndex].BulletID;
					m_Projectile[projNum].ExplodeID		= g_SpellInfo[m_SelectionIndex].ExplodeID;
					m_Projectile[projNum].ExplodeSound	= g_SpellInfo[m_SelectionIndex].ExplodeSound;
					m_Projectile[projNum].BaseTime		= 0;
					m_Projectile[projNum].Active		= 1;
					m_Projectile[projNum].Brightness	= (g_AnimInfo[g_SpellInfo[m_SelectionIndex].BulletID].Brightness * c_FixedPointMask) / 100;

					m_Projectile[projNum].Position[0]	= m_Position[0] - (m_DirectionSine/4);
					m_Projectile[projNum].Position[1]	= m_Position[1] - (c_EyeLevel - c_BulletLevel);
					m_Projectile[projNum].Position[2]	= m_Position[2] + (m_DirectionCosine/4);
				}

				// Light/Darkness spells toggle global illumination model.
				if (1 == g_SpellInfo[m_SelectionIndex].Special) {
					m_FullBright = false;
				}
				if (2 == g_SpellInfo[m_SelectionIndex].Special) {
					m_FullBright = true;
				}

				// Fog/Fresh Air spells switch between fading dark objects
				// towards black or the fog color.
				if (3 == g_SpellInfo[m_SelectionIndex].Special) {
					m_UseFogMap = true;
				}
				if (4 == g_SpellInfo[m_SelectionIndex].Special) {
					m_UseFogMap = false;
				}

				// If there is an attached casting animation, start playing
				// it, along with any associated sound effect.
				if (Casting_Nothing != m_SpellAnimation) {
					m_SpellCastBaseTime = m_CurrentTime;

					m_SoundDriver.QueueSound(Sound_SlowBall);
				}
			}
		}
		else if (Pause_SelectItem == m_PauseState) {
			m_SelectionCompleted = false;
			m_PauseState         = Pause_NotPaused;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PressEscKey()
//
//	This responds to the user hitting the ESC key.  Normally this will pop
//	up the control panel, but may instead just close the current screen
//	and return to the 3D game screen.
//
//	FIXME: entering control panel mode needs to remember the previous screen
//	and switch back to that instead of always dropping the player back into
//	the 3D world screen
//
void CwaWorld::PressEscKey(void)
{
	switch (m_DisplayMode) {
		case Display_Automap:
            m_DisplayMode = Display_Game;
			break;

		case Display_Character:
            m_DisplayMode = Display_Game;
			break;

		case Display_ControlPanel:
            m_DisplayMode = Display_Game;
			break;

		case Display_Game:
			// When paused to select or do something, pressing ESC will undo
			// the pause can cancel the action.
			if (Pause_NotPaused != m_PauseState) {
				m_PauseState = Pause_NotPaused;
			}


			// Otherwise, pop up the control panel.
			else {
				m_DisplayMode = Display_ControlPanel;
			}
			break;

		case Display_Journal:
            m_DisplayMode = Display_Game;
			break;

		case Display_Inventory:
            m_DisplayMode = Display_ControlPanel;
			break;

		case Display_LoadSave:
			m_DisplayMode = Display_ControlPanel;
			break;

		case Display_SpellBook:
            m_DisplayMode = Display_ControlPanel;
			break;

		case Display_SpellPage:
            m_DisplayMode = Display_ControlPanel;
			break;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	Random32()
//
DWORD CwaWorld::Random32(DWORD maxValue)
{
	// FIXME: add mersenne code
	return rand() % maxValue;
}


/////////////////////////////////////////////////////////////////////////////
//
//	CheckCritterCollision()
//
//	Returns true if (x,y) is within the collision radius of a monster.
//	If there was a collision, also returns the ID of the nearest monster
//	to the given point in case two monsters are close together.
//
bool CwaWorld::CheckCritterCollision(long x, long z, DWORD &id)
{
	bool found   = false;
	long nearest = 0;

	for (DWORD i = 0; i < ArraySize(m_Critter); ++i) {
		// Only test agaist critters that are alive.
		if (m_Critter[i].State >= CritterState_Alive) {
			long dx = m_Critter[i].Position[0] - x;
			long dz = m_Critter[i].Position[2] - z;

			if ((abs(dx) < c_MinCollisionDistance) && (abs(dz) < c_MinCollisionDistance)) {
				long distance = FixedDistance(dx, dz);
				if (distance < c_MinCollisionDistance) {
					if ((false == found) || (distance < nearest)) {
						id      = i;
						found   = true;
						nearest = distance;
					}
				}
			}
		}
	}

	return found;
}


/////////////////////////////////////////////////////////////////////////////
//
//	CheckHitCritterMelee()
//
bool CwaWorld::CheckHitCritterMelee(DWORD &id)
{
	bool found   = false;
	long nearest = 0;

	const c_WeaponReach = c_FixedPointOne;

	for (DWORD i = 0; i < ArraySize(m_Critter); ++i) {
		// Only test agaist critters that are alive.
		if (m_Critter[i].State >= CritterState_Alive) {

			// Rotate the position of the critter according to the player's
			// orientation.  If the critter ends up directly in front of the
			// player, it counts as a potiential target.

			long dx = m_Critter[i].Position[0] - m_Position[0];
			long dz = m_Position[2] - m_Critter[i].Position[2];

			long x = FixedMul(dx, m_DirectionCosine) - FixedMul(dz, m_DirectionSine);
			long z = FixedMul(dx, m_DirectionSine)   + FixedMul(dz, m_DirectionCosine);

			// Weapon collision test based upon simple hit box directly in
			// front of player.  If the monster is to far off to either side,
			// or too far away, the weapon can't touch it.
			if ((z > 0) &&
				(z < c_WeaponReach) &&
				(x > -(c_FixedPointOne >> 2)) &&
				(x < (c_FixedPointOne >> 2)))
			{
				// Pick the closest monster if more than one is in the hit box.
				// Note that this is not fully accurate.  In Arena, if there was
				// more than one critter right in front of them player, melee
				// attacks would hit all of them (was this only for slashes?).
				if ((false == found) || (z < nearest)) {
					id      = i;
					found   = true;
					nearest = z;
				}
			}
		}
	}

	return found;
}


/////////////////////////////////////////////////////////////////////////////
//
//	ShowOutsideOfBlock()
//
bool CwaWorld::ShowOutsideOfBlock(DWORD south, DWORD east, DWORD level, BlockSide_t &side)
{
	CellNode_t &node = m_Nodes[south][east];

	side.yHi = c_WallHeight;
	side.yLo = 0;
	side.vHi = 0;
	side.vLo = c_FixedPointOne; 

	if (1 == level) {
		if (CellFlag_Door & node.Flags) {
			return true;
		}

		if (CellFlag_SecretDoor & node.Flags) {
			long angle = PortalAngle(m_Triggers[node.Trigger & 0xFF]);
			long shift = FixedMul(c_WallHeight, angle);

			side.yHi = shift;
			side.vHi = c_FixedPointOne - angle;

			return (side.yLo != side.yHi);
		}

		if (CellFlag_Raised & node.Flags) {

			// Both adjoining blocks are raised, so no wall should be placed
			// between them.
			if (CellFlag_Raised & side.Flags) {
				return false;
			}

			side.yLo = c_WallHeight >> 2;
			side.vLo = IntToFixed(3) >> 2;

			return true;
		}

		if ((CellFlag_Raised & side.Flags) && (0 == (CellFlag_Solid_L1 & node.Flags))) {
			side.yHi = c_WallHeight >> 2;
			side.vHi = IntToFixed(3) >> 2;

			return true;
		}

		if (CellFlag_SignMask & side.Flags) {
			return false;
		}
	}

	return (0 == ((CellFlag_Solid_L0 << level) & node.Flags));
}


/////////////////////////////////////////////////////////////////////////////
//
//	IsBlockSolid()
//
bool CwaWorld::IsBlockSolid(DWORD south, DWORD east, DWORD level)
{
	CellNode_t &node = m_Nodes[south][east];

	if (1 == level) {
		// Doors are considered to be solid unless completely open.
		if ((CellFlag_Door | CellFlag_SecretDoor) & node.Flags) {
			return (0 == (TriggerFlag_Open & m_Triggers[node.Trigger].Flags));
		}

		// Sign cells are always open, but display a transparent quad on one
		// side of the cell.
		if (CellFlag_SignMask & node.Flags) {
			return false;
		}
	}


	return (0 != ((CellFlag_Solid_L0 << level) & node.Flags));
}


/////////////////////////////////////////////////////////////////////////////
//
//	CellIsVisible()
//
bool CwaWorld::CellIsVisible(DWORD south, DWORD east)
{
	CellNode_t &node = m_Nodes[south][east];

	if (CellFlag_Door & node.Flags) {
		return true;
	}
	if (CellFlag_SecretDoor & node.Flags) {
		return (0 != (TriggerFlag_StateMask & m_Triggers[node.Trigger].Flags));
	}

	return (0 == (CellFlag_Solid_L1 & node.Flags));
}


/////////////////////////////////////////////////////////////////////////////
//
//	CellTrigger()
//
DWORD CwaWorld::CellTrigger(DWORD south, DWORD east)
{
	if (CellFlag_TriggerMask & m_Nodes[south][east].Flags) {
		return (c_Shape_Cell | m_Nodes[south][east].Trigger);
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetPlayerDirection()
//
//	Updates the direction the player is facing, along with the sine/cosine
//	multipliers used for movement and direction-based processing (especially
//	3D geometric transforms).
//
void CwaWorld::SetPlayerDirection(long direction)
{
	m_Direction = direction & c_FixedPointMask;

	float radians = float(m_Direction) * 2.0f * 3.14159f / float(c_FixedPointOne);
	m_DirectionSine   = FloatToFixed(sinf(radians));
	m_DirectionCosine = FloatToFixed(cosf(radians));
}


/////////////////////////////////////////////////////////////////////////////
//
//	MovePlayer()
//
//	Moves the player the specified distance along the current m_Direction.
//	Basic collision detection/response is applied to avoid getting too close
//	to a wall, which causes clipping against the near plane (allowing player
//	to see through the wall).  Also prevents the player from moving through
//	monsters.
//
//	FIXME: Need to review this code to see how it behaves when wedged between
//	a creature and a wall.
//
void CwaWorld::MovePlayer(DWORD milliseconds)
{
	PROFILE(PFID_World_MovePlayer);

	if (Elevation_Jumping != m_Elevation) {
		SetPlayerDirection(m_Direction + ((m_TurnSpeed * long(milliseconds)) / 3000));
	}

	long distance = (m_MoveSpeed   * long(milliseconds)) / 1000;
	long strafe   = (m_StrafeSpeed * long(milliseconds)) / 1000;

	// When falling, normal movement is disabled until after landing.
	if (Elevation_FallingIntoTrench == m_Elevation) {
		distance = 0;
		strafe   = 0;
	}

	// Update the player's current position.
	m_Position[0] += (FixedMul(distance, m_DirectionSine)   * 3) / 2;
	m_Position[2] -= (FixedMul(distance, m_DirectionCosine) * 3) / 2;

	m_Position[0] += FixedMul(strafe, m_DirectionCosine);
	m_Position[2] += FixedMul(strafe, m_DirectionSine);

	// First test for any collisions with creatures.  This will push the
	// player directly away from the creature, possibly pushing the player
	// into a wall.
	for (DWORD critterNum = 0; critterNum < ArraySize(m_Critter); ++critterNum) {
		if (m_Critter[critterNum].State >= CritterState_Alive) {
			PushPlayerBack(m_Critter[critterNum].Position[0], m_Critter[critterNum].Position[2], c_CollisionLoFrac);
		}
	}

	// Figure out which cell the player is currently in.
	long east  = m_Position[0] >> c_FixedPointBits;
	long south = m_Position[2] >> c_FixedPointBits;

	// Need the fractional portion of the player's position within the cell.
	// This is used to simplify testing bounds limits against each wall or
	// corner.
	long eastFrac  = m_Position[0] & c_FixedPointMask;
	long southFrac = m_Position[2] & c_FixedPointMask;

	DWORD playerLevel = 0;

	switch (m_Elevation) {
		case Elevation_Trench:
			playerLevel   = 0;
			m_Position[1] = c_FixedPointOne - c_WaterLevel;
			break;

		case Elevation_TrenchClimbing:
			{
				long delta = m_CurrentTime - m_ElevationBaseTime;

				if (delta > 2250) {
					m_Elevation = Elevation_Floor;
					playerLevel = 1;
					return;
				}
				else if ((0 == m_MoveSpeed) || (0 != m_StrafeSpeed) || (0 != m_TurnSpeed)) {
					if (delta >= 2000) {
						m_Elevation = Elevation_Floor;
					}
					else {
						FallIntoTrench(southFrac, eastFrac, south, east);
					}
					return;
				}
				else if (delta >= 2000) {
					playerLevel   = 1;

					delta -= 2000;

					m_Position[0] = (((m_ElevationEndPos[0] - m_ElevationBeginPos[0]) * delta) / 250) + m_ElevationBeginPos[0];
					m_Position[1] = c_FixedPointOne + c_EyeLevel;
					m_Position[2] = (((m_ElevationEndPos[2] - m_ElevationBeginPos[2]) * delta) / 250) + m_ElevationBeginPos[2];

					return;
				}
				else {
					playerLevel = 0;

					m_Position[0] = m_ElevationBeginPos[0];
					m_Position[1] = (((m_ElevationEndPos[1] - m_ElevationBeginPos[1]) * delta) / 2000) + m_ElevationBeginPos[1];
					m_Position[2] = m_ElevationBeginPos[2];

					return;
				}
			}
			break;

		case Elevation_Floor:
			playerLevel   = 1;
			m_Position[1] = c_FixedPointOne + c_EyeLevel;
			break;

		case Elevation_FloorClimbing:
			playerLevel   = 1;
			break;

		case Elevation_Platform:
			playerLevel   = 1;
			break;

		case Elevation_Jumping:
			{
				playerLevel   = 1;
				m_Position[1] = c_FixedPointOne + c_EyeLevel;

				long delta = long(m_CurrentTime - m_ElevationBaseTime) - 500;
				if (delta > 500) {
					m_Elevation = Elevation_Floor;
				}
				else {
					const long maxHeight = c_FixedPointOne / 5;

					m_Position[1] += maxHeight - (((delta * delta) * (maxHeight / 100)) / (500 * 5));
				}
			}
			break;

		case Elevation_FallingIntoTrench:
			playerLevel = 1;
#define fallDuration 300
			if ((m_CurrentTime - m_ElevationBaseTime) >= fallDuration) {
				m_Position[0] = m_ElevationEndPos[0];
				m_Position[1] = m_ElevationEndPos[1];
				m_Position[2] = m_ElevationEndPos[2];
				m_Elevation = Elevation_Trench;

				m_SoundDriver.QueueSound(Sound_Splash);
			}
			else {
				long delta = m_CurrentTime - m_ElevationBaseTime;

				long horizLerp = fallDuration - (((fallDuration - delta) * (fallDuration - delta)) / fallDuration);
				long vertiLerp = (delta * delta) / fallDuration;

				m_Position[0] = (((m_ElevationEndPos[0] - m_ElevationBeginPos[0]) * horizLerp) / fallDuration) + m_ElevationBeginPos[0];
				m_Position[1] = (((m_ElevationEndPos[1] - m_ElevationBeginPos[1]) * vertiLerp) / fallDuration) + m_ElevationBeginPos[1];
				m_Position[2] = (((m_ElevationEndPos[2] - m_ElevationBeginPos[2]) * horizLerp) / fallDuration) + m_ElevationBeginPos[2];
			}
			break;
	}

	// FIXME: insert logic for jumping, platforms, and climbing out of
	// water or chasms
/*
	// Default to eye-level below the floor.
	m_Position[1] = c_FixedPointOne - (c_FixedPointOne / 5);

	// If there is a floor, eye-level is at normal height.
	if (0 != m_Nodes[south][east].Volume[0]) {
		m_Position[1] = c_FixedPointOne + c_EyeLevel;
	}
*/
	bool canClimb   = false;
	bool isOnGround = (0 != (CellFlag_Solid_L0 & m_Nodes[south][east].Flags));
	long climbX     = 0;
	long climbZ     = 0;

	if ((false == isOnGround) && (Elevation_Floor == m_Elevation)) {
		FallIntoTrench(southFrac, eastFrac, south, east);
	}

	// If there is a solid block to the north, push the player to the south
	// to maintain minimum permissible distance.
	if (IsBlockSolid(south-1, east, playerLevel)) {
		if (southFrac <= c_CollisionLoFrac) {
			if ((m_Nodes[south-1][east].Texture[1] == m_LevelDownID) ||
				(m_Nodes[south-1][east].Texture[1] == m_LevelUpID))
			{
			}

			m_Position[2] = m_Position[2] - southFrac + c_CollisionLoFrac;
			southFrac = c_CollisionLoFrac;

			if ((m_Direction < (IntToFixed(1) >> 5)) || (m_Direction > (IntToFixed(31) >> 5))) {
				canClimb = true;
				climbZ   = -c_MinCollisionDistance;

				if (((CellFlag_Solid_L1 & m_Nodes[south][east-1].Flags) && (eastFrac < c_CollisionLoFrac)) ||
					((CellFlag_Solid_L1 & m_Nodes[south][east+1].Flags) && (eastFrac > c_CollisionHiFrac)))
				{
					canClimb = false;
				}
				if (CellFlag_Solid_L1 & m_Nodes[south-1][east].Flags) {
					climbZ = 0;
				}
			}
		}
	}
	// Check for a wall to the south.
	if (IsBlockSolid(south+1, east, playerLevel)) {
		if (southFrac >= c_CollisionHiFrac) {
			if ((m_Nodes[south+1][east].Texture[1] == m_LevelDownID) ||
				(m_Nodes[south+1][east].Texture[1] == m_LevelUpID))
			{
				char *textureSets[11] =
				{
					"dcn.inf",
					"dcr.inf",
					"dcw.inf",
					"mcn.inf",
					"mcr.inf",
					"mcs.inf",
					"mcw.inf",
					"tcn.inf",
					"tcr.inf",
					"tcs.inf",
					"tcw.inf",
				};

//				m_MidiPlayer.QueueStop();
				m_MidiPlayer.QueueSong(&m_SongList[Song_SunnyDay], &m_MidiEvent, 1, false);
				BuildDemoDungeon("DemoCity.dat", textureSets[rand()%11], 88, 88, true);
				return;
			}

			m_Position[2] = m_Position[2] - southFrac + c_CollisionHiFrac;
			southFrac = c_CollisionHiFrac;

			if ((m_Direction > (IntToFixed(15) >> 5)) && (m_Direction < (IntToFixed(17) >> 5))) {
				canClimb = true;
				climbZ   = c_MinCollisionDistance;

				if (((CellFlag_Solid_L1 & m_Nodes[south][east-1].Flags) && (eastFrac < c_CollisionLoFrac)) ||
					((CellFlag_Solid_L1 & m_Nodes[south][east+1].Flags) && (eastFrac > c_CollisionHiFrac)))
				{
					canClimb = false;
				}
				if (CellFlag_Solid_L1 & m_Nodes[south+1][east].Flags) {
					climbZ = 0;
				}
			}
		}
	}

	// Check for a wall to the west.
	if (IsBlockSolid(south, east-1, playerLevel)) {
		if (eastFrac <= c_CollisionLoFrac) {
			if ((m_Nodes[south][east-1].Texture[1] == m_LevelDownID) ||
				(m_Nodes[south][east-1].Texture[1] == m_LevelUpID))
			{
			}

			m_Position[0] = m_Position[0] - eastFrac + c_CollisionLoFrac;
			eastFrac = c_CollisionLoFrac;

			if ((m_Direction > (IntToFixed(23) >> 5)) && (m_Direction < (IntToFixed(25) >> 5))) {
				canClimb = true;
				climbX   = -c_MinCollisionDistance;

				if (((CellFlag_Solid_L1 & m_Nodes[south-1][east].Flags) && (southFrac < c_CollisionLoFrac)) ||
					((CellFlag_Solid_L1 & m_Nodes[south+1][east].Flags) && (southFrac > c_CollisionHiFrac)))
				{
					canClimb = false;
				}
				if (CellFlag_Solid_L1 & m_Nodes[south][east-1].Flags) {
					climbX = 0;
				}
			}
		}
	}

	// Check for a wall to the east.
	if (IsBlockSolid(south, east+1, playerLevel)) {
		if (eastFrac >= c_CollisionHiFrac) {
			if ((m_Nodes[south][east-1].Texture[1] == m_LevelDownID) ||
				(m_Nodes[south][east-1].Texture[1] == m_LevelUpID))
			{
			}

			m_Position[0] = m_Position[0] - eastFrac + c_CollisionHiFrac;
			eastFrac = c_CollisionHiFrac;

			if ((m_Direction > (IntToFixed(7) >> 5)) && (m_Direction < (IntToFixed(9) >> 5))) {
				canClimb = true;
				climbX   = c_MinCollisionDistance;

				if (((CellFlag_Solid_L1 & m_Nodes[south-1][east].Flags) && (southFrac < c_CollisionLoFrac)) ||
					((CellFlag_Solid_L1 & m_Nodes[south+1][east].Flags) && (southFrac > c_CollisionHiFrac)))
				{
					canClimb = false;
				}
				if (CellFlag_Solid_L1 & m_Nodes[south][east+1].Flags) {
					climbX = 0;
				}
			}
		}
	}

	if ((canClimb) &&
		(0 == playerLevel) &&
		(false == IsBlockSolid(south, east, 1)) &&
		(m_MoveSpeed > 0) &&
		(0 == m_StrafeSpeed) &&
		(0 == m_TurnSpeed))
	{
		if (Elevation_Trench == m_Elevation) {
			m_Elevation         = Elevation_TrenchClimbing;
			m_ElevationBaseTime = m_CurrentTime;

			m_ElevationBeginPos[0] = m_Position[0];
			m_ElevationBeginPos[1] = m_Position[1];
			m_ElevationBeginPos[2] = m_Position[2];

			m_ElevationEndPos[0] = m_Position[0] + climbX + (climbX >> 1);
			m_ElevationEndPos[1] = c_FixedPointOne + c_EyeLevel;
			m_ElevationEndPos[2] = m_Position[2] + climbZ + (climbZ >> 1);

			SetDisplayMessage("Climbing");
		}
	}
	else if (Elevation_TrenchClimbing == m_Elevation) {
		FallIntoTrench(southFrac, eastFrac, south, east);
	}

	// Now check against the corners.  If there is no wall to the north and
	// east, check to see if there is on northeast.  If so, push the player
	// back away from that corner along the current direction vector relative
	// to the corner.  This allows the player to roll around corners, rather
	// than getting stuck on them, or getting too close and allowing the
	// walls to be visibly clicked to the near plane.

	// Northwest
	if (!IsBlockSolid(south  , east-1, playerLevel) &&
		!IsBlockSolid(south-1, east  , playerLevel) &&
		 IsBlockSolid(south-1, east-1, playerLevel))
	{
		PushPlayerBack(IntToFixed(east), IntToFixed(south), c_CollisionLoFrac);
	}

	// Northeast
	if (!IsBlockSolid(south  , east+1, playerLevel) &&
		!IsBlockSolid(south-1, east  , playerLevel) &&
		 IsBlockSolid(south-1, east+1, playerLevel))
	{
		PushPlayerBack(IntToFixed(east+1), IntToFixed(south), c_CollisionLoFrac);
	}

	// Southwest
	if (!IsBlockSolid(south  , east-1, playerLevel) &&
		!IsBlockSolid(south+1, east  , playerLevel) &&
		 IsBlockSolid(south+1, east-1, playerLevel))
	{
		PushPlayerBack(IntToFixed(east), IntToFixed(south+1), c_CollisionLoFrac);
	}

	// Southeast
	if (!IsBlockSolid(south  , east+1, playerLevel) &&
		!IsBlockSolid(south+1, east  , playerLevel) &&
		 IsBlockSolid(south+1, east+1, playerLevel))
	{
		PushPlayerBack(IntToFixed(east+1), IntToFixed(south+1), c_CollisionLoFrac);
	}


	// Keep track of the distance moved.  This ignores push-back distances
	// from collisions, so walking into a wall still causes foot steps.
	// If the distance is large enough, trigger a footstep sound effect.

	// FIXME: if jumping, don't make footstep sounds, but do when landing

	const DWORD stepInterval = IntToFixed(5) / 8;

	m_WalkDistance += abs(distance) + abs(strafe);

	if (m_WalkDistance >= stepInterval) {
		m_WalkDistance -= stepInterval;

		// Alternate between left and right footstep sounds.
		m_LeftFootStep = !m_LeftFootStep;

		// FIXME: need to distinguish against chasms vs. water
		// FIXME: what sound to play when swimming in lava?

		// On normal flooring, make footstep sounds.
		if (m_Position[1] >= c_FixedPointOne) {
			if (Elevation_Floor == m_Elevation) {
				m_SoundDriver.QueueSound(m_LeftFootStep ? Sound_DirtLeft : Sound_DirtRight);
			}
		}
		else {
			if (Elevation_Trench == m_Elevation) {
				m_SoundDriver.QueueSound(Sound_Swimming);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	FallIntoTrench()
//
void CwaWorld::FallIntoTrench(long southFrac, long eastFrac, long south, long east)
{
	m_Elevation = Elevation_FallingIntoTrench;

	m_ElevationBeginPos[0] = m_Position[0];
	m_ElevationBeginPos[1] = m_Position[1];
	m_ElevationBeginPos[2] = m_Position[2];

	m_ElevationEndPos[0] = m_Position[0];
	m_ElevationEndPos[1] = c_FixedPointOne - c_WaterLevel;
	m_ElevationEndPos[2] = m_Position[2];

	m_ElevationBaseTime = m_CurrentTime;

	if (CellFlag_Solid_L0 & m_Nodes[south-1][east].Flags) {
		if (southFrac < c_CollisionLoFrac) {
			m_ElevationEndPos[2] += c_CollisionLoFrac - southFrac;
		}
	}

	if (CellFlag_Solid_L0 & m_Nodes[south+1][east].Flags) {
		if (southFrac > c_CollisionHiFrac) {
			m_ElevationEndPos[2] += c_CollisionHiFrac - southFrac;
		}
	}

	if (CellFlag_Solid_L0 & m_Nodes[south][east-1].Flags) {
		if (eastFrac < c_CollisionLoFrac) {
			m_ElevationEndPos[0] += c_CollisionLoFrac - eastFrac;
		}
	}

	if (CellFlag_Solid_L0 & m_Nodes[south][east+1].Flags) {
		if (eastFrac > c_CollisionHiFrac) {
			m_ElevationEndPos[0] += c_CollisionHiFrac - eastFrac;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	Jump()
//
void CwaWorld::Jump(void)
{
	if ((Pause_NotPaused == m_PauseState) && (Display_Game == m_DisplayMode)) {
		if (Elevation_Floor == m_Elevation) {
			m_Elevation         = Elevation_Jumping;
			m_ElevationBaseTime = m_CurrentTime;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetTurnSpeed()
//
void CwaWorld::SetTurnSpeed(long speed)
{
	if ((Pause_NotPaused == m_PauseState) && (Display_Game == m_DisplayMode)) {
		m_TurnSpeed = IntToFixed(speed);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetMoveSpeed()
//
void CwaWorld::SetMoveSpeed(long speed)
{
	if ((Pause_NotPaused == m_PauseState) && (Display_Game == m_DisplayMode)) {
		m_MoveSpeed   = IntToFixed(speed);
		m_StrafeSpeed = 0;
	}
}

/*
/////////////////////////////////////////////////////////////////////////////
//
//	TurnPlayer()
//
//	Used by the main app code to turn the player left or right, along with
//	indication of how much time has passed since the previous frame so the
//	change in direction can be properly scaled.
//
void CwaWorld::TurnPlayer(DWORD milliseconds, bool left)
{
	// If the game is paused, or not displaying the 3D screen, ignore it.
	if ((Pause_NotPaused == m_PauseState) && (Display_Game == m_DisplayMode)) {

		long angle = left ? (-c_MaxTurnRate * milliseconds) : (c_MaxTurnRate * milliseconds);

		TurnPlayer(angle);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	TurnPlayer()
//
//	Internal version of TurnPlayer() used by mouse movement controls.
//
void CwaWorld::TurnPlayer(long angle)
{
	SetPlayerDirection((m_Direction + angle) & c_FixedPointMask);
}
*/

/////////////////////////////////////////////////////////////////////////////
//
//	CheckMouseMovement()
//
//	Check if any mouse-controlled movement should be applied.  If the mouse
//	is in the 3D part of the screen, and the game is not paused, and the
//	player isn't holding a keyboard movement key, then we will move the
//	player according to the current move and turn rates.
//
//	The mouse move/turn rates are recomputed in RenderGameScreen() when it
//	does mouse position testing.
//
void CwaWorld::CheckMouseMovement(void)
{
	if ((Display_Game == m_DisplayMode) && (Pause_NotPaused == m_PauseState)) {
		m_TurnSpeed   = m_MouseTurnRate;
		m_MoveSpeed   = m_MouseWalkRate;
		m_StrafeSpeed = m_MouseStrafeRate;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PushPlayerBack()
//
//	Given a point in the world, and a minimum distance from the point, this
//	code will push the player directly back from that point if too close.
//	This is used to resolve collisions.  If the player gets too close to a
//	corner or obstacle, the code will figure out what direction the player
//	is from that point, and push the player directly away from the point to
//	maintain the minimum requested distance.
//
void CwaWorld::PushPlayerBack(long east, long south, long minDistance)
{
	PROFILE(PFID_World_PushPlayerBack);

	long de = m_Position[0] - east;
	long ds = m_Position[2] - south;

	// FIXME: use FixedDistance()

	// Distance squared of player to the point.
	long square = FixedMul(de, de) + FixedMul(ds, ds);

	// Compute the square of the minimum distance.  This allows testing
	// with the squared values so we only need to compute the square root
	// if a collision is detected.
	long minSquare = FixedMul(minDistance, minDistance);

	if (square < minSquare) {
		long root = FixedSqrt(square);

		// Protect against div-zero errors.  This is the wrong way to
		// do it, since it will push the player in the wrong direction.
		// But assuming restrictions are placed upon how much a player
		// can move (default is 0.25 units), and how close the player
		// can get to something (any value of minDistance > 0.25), this
		// case should never happen.
		if (root == 0) {
			de   = 1;
			ds   = 0;
			root = 1;
		}

		// Normalize the vector, so we know which direction to push
		// the player.
		de = FixedDiv(de, root);
		ds = FixedDiv(ds, root);

		// Scale by minimum distance player can be from a wall.
		de = FixedMul(de, c_CollisionLoFrac);
		ds = FixedMul(ds, c_CollisionLoFrac);

		// Push the player back away from the all along this vector.
		// It's easier to explicitly replace the coordinates than try to
		// add relative offsets to achieve the move.
		m_Position[0] = east  + de;
		m_Position[2] = south + ds;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ClickOnID()
//
//	Responds to the player clicking on some object in the 3D world.  This
//	will be identified by a unique ID that tells us what was clicked.
//
void CwaWorld::ClickOnID(DWORD pointID)
{
	switch (pointID & c_Shape_TypeMask) {
		case c_Shape_Cell:
			{
				// FIXME: add passwall support

				// If the player clicked on a cell, check whether there is
				// a trigger associated with that cell.  This will normally
				// be used to open doors.
				DWORD id = pointID & 0xFF;
				if (CellFlag_Portal & m_Triggers[id].Flags) {
	BuildDemoDungeon("DemoCity.dat", "tcn.inf", 88, 88, true);
//					BuildTestCity();
				}
				else if (0 == (TriggerFlag_StateMask & m_Triggers[id].Flags)) {
					Trigger_t &trigger = m_Triggers[id];

					trigger.Flags    |= TriggerFlag_Opening;
					trigger.BaseTime  = m_CurrentTime;
					if (CellFlag_Door & m_Nodes[trigger.South][trigger.East].Flags) {
						m_SoundDriver.QueueSound(Sound_OpenDoor);
					}
					else {
						m_SoundDriver.QueueSound(Sound_Portcullis);
					}
				}
			}
			break;

		case c_Shape_Critter:
			sprintf(m_MessageBuffer, "You see a %s...",  m_Critter[pointID & 0xFFF].Name);
			SetDisplayMessage();
			break;

		case c_Shape_Sprite:
			if (SpriteFlag_Loot & m_Sprite[pointID & 0xFFF].Flags) {
				m_PauseState = Pause_SelectLoot;

				m_SelectionCompleted = false;
				m_SelectionCount     = 1;
				m_SelectionMaxVisi   = 7;
				m_SelectionIndex     = 0;
				m_SelectionOffset    = 0;
				m_SelectionID        = pointID;

				m_SelectionList[0] = "bag of gold";
				m_SelectionList[1] = "dude";
				m_SelectionList[2] = "rocksome";
				m_SelectionList[3] = "nipplicious";
				m_SelectionList[4] = "geeksome";
				m_SelectionList[5] = "wowsers";
				m_SelectionList[6] = "I meant to do that";
				m_SelectionList[7] = "hey chief";
			}
			else {
				sprintf(m_MessageBuffer, "You see %s...", m_Sprite[pointID & 0xFFF].Name);
				SetDisplayMessage();
			}
			break;

		default:
			sprintf(m_MessageBuffer, "0x%08X", pointID);
			break;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetDisplayMessage()
//
//	Resets the info related to the text message stored in m_MessageBuffer.
//	Messages will be displayed for a period of time.  Need to know the base
//	time the message was displayed, and how large the message text is so
//	we can center the text on the screen.
//
//	The contents of m_MessageBuffer[] should have already been updated before
//	calling this method.  If not, provide a new message via the pMessage
//	parameter.
//
void CwaWorld::SetDisplayMessage(char *pMessage)
{
	if (NULL != pMessage) {
		strncpy(m_MessageBuffer, pMessage, ArraySize(m_MessageBuffer));
		m_MessageBuffer[ArraySize(m_MessageBuffer)-1] = '\0';
	}

	m_MessageBaseTime = m_CurrentTime;
	m_MessageWidth    = m_pRaster->StringLength(m_Loader.GetFont(Font_Arena), m_MessageBuffer);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ResetTriangleQueues()
//
//	Resets all of the queue pointers.  The input and output queues are reset
//	to empty, while all triangles are moved into the free queue.
//
void CwaWorld::ResetTriangleQueues(void)
{
	m_pInputTriangleQueue  = NULL;
	m_pOutputTriangleQueue = NULL;
	m_pFreeTriangleQueue   = &m_TriangleList[0];

	for (DWORD i = 0; i < ArraySize(m_TriangleList) - 1; ++i) {
		m_TriangleList[i].pNext = &m_TriangleList[i+1];
	}

	m_TriangleList[ArraySize(m_TriangleList) - 1].pNext = NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SwapInputOutputTriangleQueues()
//
//	Frees any triangles remaining on the input queue, then moves all triangles
//	from the output queue to the input queue.
//
void CwaWorld::SwapInputOutputTriangleQueues(void)
{
	Triangle_t *pTriangle;

	while (NULL != (pTriangle = PopInputTriangle())) {
		FreeTriangle(pTriangle);
	}

	m_pInputTriangleQueue  = m_pOutputTriangleQueue;
	m_pOutputTriangleQueue = NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
//	NewTriangle()
//
//	Allocates a triangle by popping one off of the free queue.  Since all
//	triangle processing is limited to a few triangles at a time, the free
//	queue does not need to be large, and should never grow.  If the queue
//	is ever empty, this will result in an access violation, which is fine
//	since something must be severely wrong before that could happen.
//
Triangle_t* CwaWorld::NewTriangle(void)
{
	Triangle_t *pTriangle = m_pFreeTriangleQueue;

	m_pFreeTriangleQueue = m_pFreeTriangleQueue->pNext;

	pTriangle->pNext = NULL;

	return pTriangle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	FreeTriangle()
//
//	Pushes an unused triangle back onto the queue of free triangles.
//
void CwaWorld::FreeTriangle(Triangle_t *pTriangle)
{
	pTriangle->pNext = m_pFreeTriangleQueue;

	m_pFreeTriangleQueue = pTriangle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	PopInputTriangle()
//
//	Returns the head of the input queue, or NULL if the queue is empty.
//
Triangle_t* CwaWorld::PopInputTriangle(void)
{
	if (NULL == m_pInputTriangleQueue) {
		return NULL;
	}

	Triangle_t *pTriangle = m_pInputTriangleQueue;
	m_pInputTriangleQueue = m_pInputTriangleQueue->pNext;

	pTriangle->pNext = NULL;

	return pTriangle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	PushOutputTriangle()
//
//	Pushes the given triangle onto the head of the output queue.
//
void CwaWorld::PushOutputTriangle(Triangle_t *pTriangle)
{
	pTriangle->pNext = m_pOutputTriangleQueue;

	m_pOutputTriangleQueue = pTriangle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	PopOutputTriangle()
//
//	Returns the head of the output queue, or NULL if the queue is empty.
//
Triangle_t* CwaWorld::PopOutputTriangle(void)
{
	if (NULL == m_pOutputTriangleQueue) {
		return NULL;
	}

	Triangle_t *pTriangle  = m_pOutputTriangleQueue;
	m_pOutputTriangleQueue = m_pOutputTriangleQueue->pNext;

	pTriangle->pNext = NULL;

	return pTriangle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	ComputeBrightness()
//
//	Returns the amount of light at a point in space.  This is all 2D lighting,
//	so changed in height are ignored.
//
//	The result is fixed-point, and should always be < 1.0 (in other words, it
//	should always be <= c_FixedPointMask).  Otherwise bitshift operations on
//	the light value will result in out-of-bounds index values.
//
long CwaWorld::ComputeBrightness(long position[])
{
	PROFILE(PFID_World_ComputeBrightness);

	// Short-circuit everything if rendering should display all geometry
	// at full illumination.
	if (m_FullBright) {
		return c_MaxLuma;
	}

	// Keep a running total of all illumination.
	long luma = 0;

	const long c_LumaDivisor         = 2;
	const long c_FullIntensityCutoff = (3 * c_FixedPointOne) / 4;
//	const long c_LightCutoff         = IntToFixed(c_LumaDivisor) + c_FullIntensityCutoff;

	// If the player casts light (which is normally true), geometry close to
	// the player needs to be illuminated.
	if (m_IlluminatePlayer) {
		long dx = m_Position[0] - position[0];
		long dz = m_Position[2] - position[2];

		if (dx < 0) { dx = -dx; }
		if (dz < 0) { dz = -dz; }

		if ((dx < IntToFixed(3)) && (dz < IntToFixed(3))) {
			long distance = FixedDistanceMacro(dx, dz);

			if (distance > c_FullIntensityCutoff) {
				long attenuation = c_FixedPointMask - ((distance - c_FullIntensityCutoff) / c_LumaDivisor);

				if (attenuation > 0) {
					luma += attenuation;
				}
			}

			// Special case: point is within full light range of player, so the
			// point is fully lit.  Drop out now since we've already saturated
			// the brightness -- no subsequent lights could make this any brighter
			// (unless negative brightness is added).
			else {
				return c_MaxLuma;
			}
		}
	}

	// Check for static light sounds in the environment.  This allows braziers
	// and torches to illuminate their surroundings.
	for (DWORD lightNum = 0; lightNum < m_LightCount; ++lightNum) {
		long dx = m_Lights[lightNum].Position[0] - position[0];
		long dz = m_Lights[lightNum].Position[2] - position[2];

		if (dx < 0) { dx = -dx; }
		if (dz < 0) { dz = -dz; }

		if ((dx < IntToFixed(3)) && (dz < IntToFixed(3))) {
			long distance = FixedDistanceMacro(dx, dz);

			if (distance > c_FullIntensityCutoff) {
				long attenuation = c_FixedPointMask - ((distance - c_FullIntensityCutoff) / c_LumaDivisor);

				if (attenuation > 0) {
					luma += FixedMul(attenuation, m_Lights[lightNum].CurrentBrightness);
				}
			}
			else {
				luma += m_Lights[lightNum].CurrentBrightness;
			}
		}
	}

	// Check for light-casting projectiles.  Most spells will cast light
	// on the environment.
	for (DWORD projNum = 0; projNum < ArraySize(m_Projectile); ++projNum) {
		if ((0 != m_Projectile[projNum].Active) && (0 != m_Projectile[projNum].Brightness)) {
			long dx = m_Projectile[projNum].Position[0] - position[0];
			long dz = m_Projectile[projNum].Position[2] - position[2];

			if (dx < 0) { dx = -dx; }
			if (dz < 0) { dz = -dz; }

			if ((dx < IntToFixed(3)) && (dz < IntToFixed(3))) {
				long distance = FixedDistanceMacro(dx, dz);

				if (distance > c_FullIntensityCutoff) {
					long attenuation = c_FixedPointMask - ((distance - c_FullIntensityCutoff) / c_LumaDivisor);

					if (attenuation > 0) {
						luma += FixedMul(attenuation, m_Projectile[projNum].Brightness);
					}
				}
				else {
					luma += m_Projectile[projNum].Brightness;
				}
			}
		}
	}

	// Need to restrict max brightness to 0.999, since fixed-point lighting
	// only looks at the fractional bits.  Values >= 1.0 would wrap and
	// appear dark, or produce out-of-bounds indices into LUTs.
	if (luma > c_FixedPointOne) {
		return c_MaxLuma;
	}

	return luma * c_LumaScale;
}


/////////////////////////////////////////////////////////////////////////////
//
//	UpdateLightSources()
//
//	Updates the state for all dynamic light sources.  Since light sources
//	are normally fires, the brightness is randomly changed to simulate a
//	flickering light.
//
//	This version will change brightness every 40-80 milliseconds (or 12-25
//	times per second).
//
//	The variation is kept to 1/6 of full intensity.  This seems to strike a
//	good balance of subtle-yet-obvious, but may be dependent upon monitor
//	brightness.
//
void CwaWorld::UpdateLightSources(void)
{
	PROFILE(PFID_World_UpdateLightSources);

	for (DWORD lightNum = 0; lightNum < m_LightCount; ++lightNum) {
		DWORD delta = m_CurrentTime - m_Lights[lightNum].FlickerTime;

		if (delta > m_Lights[lightNum].FlickerDuration) {
			long scale = rand() & c_FixedPointMask;
			scale = (scale / 6) + ((5 * c_FixedPointMask) / 6);

			m_Lights[lightNum].FlickerTime       = m_CurrentTime;
			m_Lights[lightNum].FlickerDuration   = 40 + (rand() % 40);
			m_Lights[lightNum].CurrentBrightness = FixedMul(m_Lights[lightNum].BaseBrightness, scale);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	UpdateMouseMoving()
//
//	Tests the position of the mouse pointer.  When in the 3D world window,
//	the mouse can be moved towards the edge of the screen.  Holding down the
//	left mouse button will cause the player to move or turn.
//
//	This code will determine how much to move or turn the player if the left
//	mouse button is depressed.  However, it does not apply those changes to
//	the player.  The actual movement is handled in AdvanceTime(), since it
//	needs a time offset (in milliseconds) to scale movement rate correctly.
//
void CwaWorld::UpdateMouseMoving(DWORD scale)
{
	long fraction			= m_MouseTurnRateFraction;

	m_MouseMoveEnabled		= false;
	m_MouseTurnRate			= 0;
	m_MouseTurnRateFraction	= 0;
	m_MouseWalkRate			= 0;
	m_MouseStrafeRate		= 0;

	DWORD mouseX = m_MouseX / scale;
	DWORD mouseY = m_MouseY / scale;

	if (mouseY < 147) {
		m_MouseMoveEnabled = true;

		if (mouseY < 41) {
			m_MouseWalkRate = ((42 - mouseY) * c_MaxMoveRate) / 42;

			if (mouseX >= 185) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[2]);
				long rate = ((mouseX - 185) * c_MaxTurnRate) + fraction;
				m_MouseTurnRate         = rate / (1350 / 8);
				m_MouseTurnRateFraction = rate % (1350 / 8);
			}
			else if (mouseX <= 135) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[0]);
				long rate = (long(mouseX - 135) * c_MaxTurnRate) + fraction;
				m_MouseTurnRate         = rate / (1350 / 8);
				m_MouseTurnRateFraction = rate % (1350 / 8);
			}
			else {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[1]);
			}
		}
		else if (mouseY < 116) {
			if (mouseX >= 235) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[5]);
				long rate				= ((mouseX - 235) * c_MaxTurnRate) + fraction;
				m_MouseTurnRate			= rate / (850 / 8);
				m_MouseTurnRateFraction	= rate % (850 / 8);
			}
			else if (mouseX <= 85) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[3]);
				long rate				= ((long(mouseX) - 85) * c_MaxTurnRate) + fraction;
				m_MouseTurnRate			= rate / (850 / 8);
				m_MouseTurnRateFraction	= rate % (850 / 8);
			}
			else {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[4]);
				m_MouseMoveEnabled = false;
			}
		}
		else {
			if (mouseX >= 185) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[8]);

				m_MouseStrafeRate = ((mouseX - 185) * c_MaxMoveRate) / 135;
			}
			else if (mouseX <= 135) {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[6]);

				m_MouseStrafeRate = (long(mouseX - 135) * c_MaxMoveRate) / 135;
			}
			else {
				m_pCurrentMousePointer = &(m_MousePointer.m_pFrames[7]);
				m_MouseWalkRate = ((115 - long(mouseY)) * c_MaxMoveRate) / (147 - 115);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	UpdateTriggers()
//
//	Updates the state of triggers.  If a trigger is in a transitional state
//	(a door is opening or closing), this will update the progress.  If a
//	closing door has finished closing, for example, this will place the door
//	in a fully closed state.
//
//	FIXME: add support for sound triggers -- some cells trigger a sound
//	effect or text message when entered
//
void CwaWorld::UpdateTriggers(void)
{
	for (DWORD triggerNum = 0; triggerNum < m_TriggerCount; ++triggerNum) {
		Trigger_t &trigger = m_Triggers[triggerNum];

		if (TriggerFlag_Opening & trigger.Flags) {
			if ((m_CurrentTime - trigger.BaseTime) > c_PortalOpenTime) {
				trigger.Flags &= ~TriggerFlag_Opening;
				trigger.Flags |= TriggerFlag_Open;
			}
		}
		else if (TriggerFlag_Closing & trigger.Flags) {
			if ((m_CurrentTime - trigger.BaseTime) > c_PortalOpenTime) {
				trigger.Flags &= ~(TriggerFlag_Closing | TriggerFlag_Open);
			}
		}
		else if (TriggerFlag_Open & trigger.Flags) {
			long de = trigger.East  - (m_Position[0] >> c_FixedPointBits);
			long ds = trigger.South - (m_Position[2] >> c_FixedPointBits);

			// Make doors automatically close if the player gets more than
			// three cells away in any direction.  This should be far enough
			// to keep the player from sprinting back to the door while it
			// is still closing.
			if ((abs(de) > 3) || (abs(ds) > 3)) {
				trigger.Flags    &= ~TriggerFlag_Open;
				trigger.Flags    |= TriggerFlag_Closing;
				trigger.BaseTime  = m_CurrentTime;
				if (CellFlag_Door & m_Nodes[trigger.South][trigger.East].Flags) {
					m_SoundDriver.QueueSound(Sound_OpenDoor, 1, 128);
				}
				else {
					m_SoundDriver.QueueSound(Sound_Portcullis, 1, 128);
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ShowArmorRatings()
//
//	Displays the armor class ratings for the different parts of the body.
//	These values are shown in the inventory and spellbook screens, but not
//	in the base character stats page.
//
void CwaWorld::ShowArmorRatings(DWORD scale)
{
	const BYTE colorArmorClass = 195;

	// Pick some arbitrary AC values for the different body parts so we have
	// something to test the positioning of the numbers on the screen.

	long head			=  4;
	long rightShoulder	=  3;
	long leftShoulder	=  2;
	long chest			=  1;
	long abdomen		=  0;
	long grieves		= -1;
	long feet			= -2;

	FontElement_t *pFont = m_Loader.GetFont(Font_Arena);

	m_pRaster->DrawSignedNumber(pFont, 205*scale,  21*scale, head,			colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 206*scale,  35*scale, rightShoulder,	colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 290*scale,  35*scale, leftShoulder,	colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 200*scale,  51*scale, chest,			colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 207*scale,  70*scale, abdomen,		colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 198*scale, 117*scale, grieves,		colorArmorClass);
	m_pRaster->DrawSignedNumber(pFont, 197*scale, 149*scale, feet,			colorArmorClass);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Render()
//
//	Base rendering function called by the top-level app code.  This requires
//	that a rasterizer already have been assigned.
//
//	Depending upon the current display mode, specific rendering functions
//	will be called to draw the screen.
//
void CwaWorld::Render(void)
{
	PROFILE(PFID_World_Render);

	if (Display_Game != m_DisplayMode) {
		m_pRaster->ClearFrameBuffer(0, 0);
	}


	// Get the scaling factor needed for GUI elements.  All GUI graphics
	// are scaled for a 320x200 screen.  Since the graphics can also be
	// rendered at 640x400 and 960x600, set <scale> to x2 or x3 so all of
	// the GUI graphics will be rendered at the correct size.
	//
	DWORD scale = m_pRaster->FrameHeight() / 200;

	// Clear the frame buffer and reset the current light map.
	m_pRaster->SetLightMap(m_UseFogMap ? m_LightMapFog : m_LightMapNormal);

	// Only some display modes will show the HUD at the bottom of the screen.
	// Default to not showing the HUD, unless a particular mode needs it.
	bool showHUD = false;

	// The mouse pointer defaults to the "arrow" icon (looks more like a
	// sword to me).  This will be changed in some modes, usually in the 3D
	// window to indicate movement directions.
	m_pCurrentMousePointer = &m_TextureList[Texture_Pointer];

	switch (m_DisplayMode) {
		case Display_Automap:
			RenderAutomap(scale);
			break;

		case Display_Character:
			RenderCharacter(scale);
			break;

		case Display_ControlPanel:
			RenderControlPanel(scale);
			showHUD = true;
			break;

		case Display_Game:
			RenderGameScreen(scale);
			showHUD = true;
			break;

		case Display_Inventory:
			RenderInventory(scale);
			break;

		case Display_Journal:
			RenderJournal(scale);
			break;

		case Display_LoadSave:
			RenderLoadSave(scale);
			showHUD = true;
			break;

		case Display_SpellBook:
			RenderSpellBook(scale);
			break;

		case Display_SpellPage:
			RenderSpellPage(scale);
			break;
	}

	// Is the HUD visible?  If so, copy it to the bottom of the screen.
	//
	// FIXME: check whether the "no camp" and "no spell" icons are used
	// (presumably in town, or for classes without spell casting abilities)
	//
	if (showHUD) {
		FontElement_t *pFont = m_Loader.GetFont(Font_Teeny);

		m_pRaster->Blit(m_TextureList[Texture_HUD], 0, 147*scale);

		m_pRaster->DrawString(pFont, 17*scale, 154*scale, m_Character.Name, 240);

		if ((Pause_SelectSpell == m_PauseState) || (Pause_SelectItem == m_PauseState)) {

			for (DWORD i = 0; i < m_SelectionMaxVisi; ++i) {
				if ((i + m_SelectionOffset) < m_SelectionCount) {
					DWORD index = i + m_SelectionOffset;
					DWORD color = (index == m_SelectionIndex) ? 252 : 253;
					m_pRaster->DrawString(pFont, 223*scale, (154 + i*8) * scale, m_SelectionList[index], color);
				}
			}
		}
	}

	// Mouse position is already scaled to frame buffer resolution, so
	// don't multiply by <scale>.
	if (NULL != m_pCurrentMousePointer) {
		m_pRaster->BlitTransparent(*m_pCurrentMousePointer, m_MouseX, m_MouseY);
	}

	if (m_ShowFPS) {
		FontElement_t *pFont = m_Loader.GetFont(Font_B);

		char message[128];

		sprintf(message, "FPS %d", m_FPS);

		m_pRaster->DrawString(pFont, 4*scale, 4*scale, message, 15);

		DWORD quadCount = m_QuadCount;

		if ((0 != m_QuadLimit) && (quadCount > m_QuadLimit)) {
			quadCount = m_QuadLimit;
		}

		sprintf(message, "Quads %d", quadCount);

		m_pRaster->DrawString(pFont, 4*scale, 16*scale, message, 15);

		if (Pause_NotPaused != m_PauseState) {
			m_pRaster->DrawString(pFont, 4*scale, 28*scale, "Paused", 15);
		}

		if (m_QuickSortOverflow) {
			m_pRaster->DrawString(pFont, 4*scale, 40*scale, "Qsort overflow", 15);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderAutomap()
//
void CwaWorld::RenderAutomap(DWORD /*scale*/)
{
	m_pRaster->Blit(m_TextureList[Texture_Automap], 0, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderCharacter()
//
//	Draws the character stats page.  This should display the background image,
//	the character race screen (along with visible equipment), and the actual
//	numerical values for stats, modifiers, level, etc.
//
//	Currently the background graphics cannot be decoded, so no background
//	images can be displayed.
//
void CwaWorld::RenderCharacter(DWORD scale)
{
	FontElement_t *pFontValue = m_Loader.GetFont(Font_Arena);
	FontElement_t *pFontNames = m_Loader.GetFont(Font_Char);

	const DWORD fontColor = 254;

	char buffer[64];
	sprintf(buffer, "%d/%d", m_Character.HealthCurrent, m_Character.HealthMaximum);

	m_pRaster->DrawString(pFontValue,  45*scale, 127*scale, buffer, fontColor);

	sprintf(buffer, "%d/%d", m_Character.FatigueCurrent, m_Character.FatigueMaximum);

	m_pRaster->DrawString(pFontValue,  45*scale, 135*scale, buffer, fontColor);

	sprintf(buffer, "%d/%d", m_Character.SpellPointsCurrent, m_Character.SpellPointsMaximum);

	m_pRaster->DrawString(pFontValue,  86*scale,  60*scale, buffer, fontColor);

	m_pRaster->DrawString(pFontValue,  11*scale,   7*scale, m_Character.Name, fontColor);
	m_pRaster->DrawString(pFontValue,  11*scale,  16*scale, m_Character.Race, fontColor);
	m_pRaster->DrawString(pFontValue,  11*scale,  25*scale, m_Character.Class, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  52*scale, m_Character.Strength, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  60*scale, m_Character.Intelligence, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  68*scale, m_Character.Willpower, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  76*scale, m_Character.Agility, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  84*scale, m_Character.Speed, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale,  92*scale, m_Character.Endurance, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale, 100*scale, m_Character.Personality, fontColor);
	m_pRaster->DrawNumber(pFontValue,  26*scale, 108*scale, m_Character.Luck, fontColor);

	m_pRaster->DrawNumber(pFontValue,  45*scale, 144*scale, m_Character.Gold, fontColor);
	m_pRaster->DrawNumber(pFontValue,  45*scale, 158*scale, m_Character.Experience, fontColor);
	m_pRaster->DrawNumber(pFontValue,  45*scale, 167*scale, m_Character.Level, fontColor);
	m_pRaster->DrawNumber(pFontValue, 145*scale,  52*scale, m_Character.MaxWeight, fontColor);

	m_pRaster->DrawSignedNumber(pFontValue,  86*scale,  52*scale, m_Character.DamageModifier, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue,  86*scale,  68*scale, m_Character.MagicDefence, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue,  86*scale,  76*scale, m_Character.ToHitModifier, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue,  86*scale,  92*scale, m_Character.HealthModifier, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue,  86*scale, 100*scale, m_Character.CharismaModifier, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue, 145*scale,  76*scale, m_Character.ArmorClassModifier, fontColor);
	m_pRaster->DrawSignedNumber(pFontValue, 145*scale,  92*scale, m_Character.HealModifier, fontColor);

	// FIXME: Remove the following text.  This all appears to be burned into
	// the background image, but the image format used there cannot currently
	// be decoded.

	m_pRaster->DrawString(pFontNames,  11*scale,  52*scale, "Str:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale,  60*scale, "Int:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale,  68*scale, "Wil:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale,  76*scale, "Agi:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale,  84*scale, "Spd:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale,  92*scale, "End:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale, 100*scale, "Per:", 253);
	m_pRaster->DrawString(pFontNames,  11*scale, 108*scale, "Luc:", 253);

	m_pRaster->DrawString(pFontNames,  18*scale, 127*scale, "Health:", 253);
	m_pRaster->DrawString(pFontNames,  16*scale, 135*scale, "Fatigue:", 253);
	m_pRaster->DrawString(pFontNames,  26*scale, 144*scale, "Gold:", 253);
	m_pRaster->DrawString(pFontNames,   4*scale, 157*scale, "Experience:", 253);
	m_pRaster->DrawString(pFontNames,  22*scale, 167*scale, "Level:", 253);
	m_pRaster->DrawString(pFontNames,  58*scale,  52*scale, "Damage:", 253);
	m_pRaster->DrawString(pFontNames,  47*scale,  60*scale, "Spell Pts:", 253);
	m_pRaster->DrawString(pFontNames,  48*scale,  68*scale, "Magic Def:", 253);
	m_pRaster->DrawString(pFontNames,  61*scale,  76*scale, "To Hit:", 253);
	m_pRaster->DrawString(pFontNames,  59*scale,  92*scale, "Health:", 253);
	m_pRaster->DrawString(pFontNames,  52*scale, 100*scale, "Charisma:", 253);
	m_pRaster->DrawString(pFontNames, 108*scale,  52*scale, "Max Kilos:", 253);
	m_pRaster->DrawString(pFontNames, 107*scale,  76*scale, "To Defend:", 253);
	m_pRaster->DrawString(pFontNames, 110*scale,  92*scale, "Heal Mod:", 253);

	m_pRaster->DrawString(pFontNames,  18*scale, 182*scale, "Done", 253);
	m_pRaster->DrawString(pFontNames, 115*scale, 182*scale, "Next Page", 253);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderControlPanel()
//
//	Displays the control panel graphic (with the "Load", "Save", sound volume
//	controls, etc.).
//
//	FIXME: make the sound volume controls work
//
void CwaWorld::RenderControlPanel(DWORD /*scale*/)
{
	m_pRaster->Blit(m_TextureList[Texture_ControlPanel], 0, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ComputeGridCoords()
//
void CwaWorld::ComputeGridCoords(GridCoords_t *pGrid, long lineNum)
{
	long coords[3];
	coords[1] = 0;
	coords[2] = IntToFixed(lineNum);

	for (DWORD i = 0; i <= m_MaxEast; ++i) {
		coords[0] = IntToFixed(i);

		long x = coords[0] - m_Position[0];
		long z = m_Position[2] - coords[2];

		pGrid[i].x = FixedMul(x, m_DirectionCosine) - FixedMul(z, m_DirectionSine);
		pGrid[i].z = FixedMul(x, m_DirectionSine)   + FixedMul(z, m_DirectionCosine);

		// Simple visibility culling, based upon view frustum.  If the point is
		// significantly outside the frustum, assign it a negative value.  This
		// allows other code to test the luma values at the corners of each
		// block -- if any of the corners is flagged with a negative luma, the
		// block is completely outside the view frustum.
		if ((pGrid[i].z > -IntToFixed(2)) && (abs(pGrid[i].x) < (pGrid[i].z + IntToFixed(2)))) {
			pGrid[i].luma = ComputeBrightness(coords);
		}
		else {
			pGrid[i].luma = 0x80000000;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderGameScreen()
//
//	Displays the 3D view of the game world.  Also responsible for updating
//	a fair bit of the world status -- most of the rest is handled within
//	AdvanceTime().
//
void CwaWorld::RenderGameScreen(DWORD scale)
{
	PROFILE(PFID_World_RenderGameScreen);

	m_QuadCount = 0;

	UpdateLightSources();
	UpdateMouseMoving(scale);
	UpdateTriggers();

	RenderWater(scale);


	GridCoords_t *pGrid0 = reinterpret_cast<GridCoords_t*>(alloca((m_MaxEast + 1) * sizeof(GridCoords_t)));
	GridCoords_t *pGrid1 = reinterpret_cast<GridCoords_t*>(alloca((m_MaxEast + 1) * sizeof(GridCoords_t)));

	ComputeGridCoords(pGrid0, 0);

	DWORD playerEast  = m_Position[0] >> c_FixedPointBits;
	DWORD playerSouth = m_Position[2] >> c_FixedPointBits;

	for (long cs = 0; cs < long(m_MaxSouth); ++cs) {
		ComputeGridCoords(pGrid1, cs + 1);

		for (long ce = 0; ce < long(m_MaxEast); ++ce) {

			// Check the luma values of the four corners of the cell.  If
			// any corner has a negative luma, it is completely outside the
			// view frustum, so we don't need to render it.
			if (0 == (0x80000000 & pGrid0[ce].luma & pGrid0[ce+1].luma & pGrid1[ce].luma & pGrid1[ce+1].luma)) {
				if ((abs(long(playerEast - ce)) < long(m_MaxDrawDistance)) &&
					(abs(long(playerSouth - cs)) < long(m_MaxDrawDistance)))
				{
					RenderCell(pGrid0, pGrid1, cs, ce);
				}
			}
		}

		Swap(pGrid0, pGrid1);
	}

	for (DWORD spriteNum = 0; spriteNum < ArraySize(m_Sprite); ++spriteNum) {
		if (SpriteFlag_Active & m_Sprite[spriteNum].Flags) {
			RenderSprite(spriteNum);
		}
	}

	for (DWORD critterNum = 0; critterNum < ArraySize(m_Critter); ++critterNum) {
		if (CritterState_FreeSlot != m_Critter[critterNum].State) {
			RenderCritter(critterNum);
		}
	}

	for (DWORD projNum = 0; projNum < ArraySize(m_Projectile); ++projNum) {
		if (m_Projectile[projNum].Active) {
			RenderProjectile(projNum);
		}
	}

	SortQuadList();
	RenderQuadList();

	RenderWeaponOverlay(scale);

	RenderSpellCasting(scale);

	// If there is a text message visible, display it.  Messages time out
	// after awhile.  For now, make it default to 3 seconds.  In Arena, the
	// timeout seems related to the number of characters in the message.
	if ('\0' != m_MessageBuffer[0]) {
		FontElement_t *pFont = m_Loader.GetFont(Font_Arena);

		if ((m_CurrentTime - m_MessageBaseTime) > 3000) {
			m_MessageBuffer[0] = '\0';
		}
		else {
			DWORD x = (160 - (m_MessageWidth / 2)) * scale;
			m_pRaster->DrawString(pFont, x-scale, 21*scale, m_MessageBuffer, 112);
			m_pRaster->DrawString(pFont, x,       21*scale, m_MessageBuffer, 159);
		}
	}


	// Display the compass border.  This is kept until last so the compass
	// will appear in front of any weapon or spell animations being overlain
	// on the screen.
	m_pRaster->BlitTransparent(m_TextureList[Texture_CompassBorder], 146*scale, 0);

	// Need to compute an offset into the compass heading.  Thankfully whoever
	// created this image replicated the left part of the image on the right
	// side, so we won't need to special-case anything for wrap-around.
	//
	// The compass line image is 288 pixels wide, with the last 32 pixels
	// duplicating the left part of the image.  The window in which the
	// compass is displayed is also 32 pixels, so we need to compute an offset
	// of 0-255.  But we need to factor in a -16, since the compass lines
	// don't start with north centered in the window.  Since m_Direction is a
	// 16.16 fixed-point value, but only uses the low 16 bits, we can get it
	// properly formatted by throwing out the low 8 bits.
	//
	DWORD compassHeading = ((m_Direction >> (c_FixedPointBits - 8)) + 256 - 16) & 0xFF;

	// Now that we know the offset into the compass line texture, we can blit
	// the correct block of 32x7 pixels onto the compass.
	m_pRaster->BlitRect(m_TextureList[Texture_CompassLines],
			149*scale, 7*scale,
			compassHeading, 0,
			32*scale, 7*scale);

	if (Pause_SelectLoot == m_PauseState) {
		m_pRaster->Blit(m_TextureList[Texture_EquipmentPopup], 56*scale, 10*scale);
/*
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 34*scale, "Sword", 252);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 42*scale, "Dagger", 253);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 50*scale, "a", 253);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 58*scale, "b", 253);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 66*scale, "c", 253);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 74*scale, "d", 253);
		m_pRaster->DrawString(g_FontTeeny, 85*scale, 82*scale, "e", 253);
*/
		for (DWORD i = 0; i < m_SelectionMaxVisi; ++i) {
			if ((i + m_SelectionOffset) < m_SelectionCount) {
				DWORD index = i + m_SelectionOffset;
				DWORD color = (index == m_SelectionIndex) ? 252 : 253;
				m_pRaster->DrawString(m_Loader.GetFont(Font_Teeny), 85*scale, (34 + i*8) * scale, m_SelectionList[index], color);
			}
		}

		m_pCurrentMousePointer = &m_TextureList[Texture_Pointer];
	}

	memcpy(m_Palette, m_Loader.GetPaletteARGB(Palette_Default), sizeof(m_Palette));

	// [1] is stored as blue, but often rendered as opaque black (whereas [0] is
	// transparent black)
	m_Palette[1] = 0x00000000;

	// [113] is used for windows, should reflect sky color, or glow yellow at night
	m_Palette[113] = 0x00FFFF00;

	m_pRaster->SetPalette(m_Palette);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderInventory()
//
void CwaWorld::RenderInventory(DWORD scale)
{
	FontElement_t *pFontValue  = m_Loader.GetFont(Font_Arena);
	FontElement_t *pFontItem   = m_Loader.GetFont(Font_Teeny);
	FontElement_t *pFontLabel1 = m_Loader.GetFont(Font_B);
	FontElement_t *pFontLabel2 = m_Loader.GetFont(Font_Char);

	// The inventory screen uses a custom palette stored in EQUIP.IMG.
	// It's also the same palette stored in CHARSHET.COL, so we can use that
	// one until the compression scheme used for IMG files is decoded.
	m_pRaster->SetPalette(m_Loader.GetPaletteARGB(Palette_CharSheet));

	const BYTE colorEquiped      =  85;
	const BYTE colorEquipable    =  88;
	const BYTE colorMagic        = 104;
	const BYTE colorMagicEquiped = 101;
	const BYTE colorUnequipable  = 160;

	m_pRaster->DrawString(pFontValue,  11*scale,   7*scale, m_Character.Name, 254);
	m_pRaster->DrawString(pFontValue,  11*scale,  16*scale, m_Character.Race, 254);
	m_pRaster->DrawString(pFontValue,  11*scale,  25*scale, m_Character.Class, 254);
	m_pRaster->DrawNumber(pFontValue, 128*scale,  23*scale, m_Character.Level, 254);

// FIXME: remove these lines when background page can be decoded
m_pRaster->DrawString(pFontLabel2, 105*scale, 23*scale, "Level:", 253);
m_pRaster->DrawString(pFontLabel1,  63*scale, 41*scale, "Equipment", 253);

	m_pRaster->DrawString(pFontItem, 14*scale, 50*scale, "Ebony Torc", colorEquiped);
	m_pRaster->DrawString(pFontItem, 14*scale, 61*scale, "Amulet", colorMagic);
	m_pRaster->DrawString(pFontItem, 14*scale, 72*scale, "Bracers", colorMagicEquiped);
	m_pRaster->DrawString(pFontItem, 14*scale, 83*scale, "Potion", colorEquipable);

	// FIXME: add logic to support selecting items, and scrolling the list

	m_pRaster->DrawBox(12*scale, 48*scale, 146*scale, 12*scale, colorUnequipable);

	if (scale>1) {
		m_pRaster->DrawBox(12*scale+1, 48*scale+1, 146*scale-2, 12*scale-2, colorUnequipable);
		if (scale>2) {
			m_pRaster->DrawBox(12*scale+2, 48*scale+2, 146*scale-4, 12*scale-4, colorUnequipable);
		}
	}

	m_pRaster->DrawString(pFontLabel2,  14*scale, 192*scale, "Exit", 253);
	m_pRaster->DrawString(pFontLabel2,  67*scale, 191*scale, "Spellbook", 253);
	m_pRaster->DrawString(pFontLabel2, 138*scale, 192*scale, "Drop", 253);

	ShowArmorRatings(scale);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderJournal()
//
void CwaWorld::RenderJournal(DWORD /*scale*/)
{
	m_pRaster->Blit(m_TextureList[Texture_Journal], 0, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderLoadSave()
//
void CwaWorld::RenderLoadSave(DWORD /*scale*/)
{
	m_pRaster->Blit(m_TextureList[Texture_LoadSave], 0, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderSpellBook()
//
void CwaWorld::RenderSpellBook(DWORD scale)
{
	FontElement_t *pFontValue  = m_Loader.GetFont(Font_Arena);
	FontElement_t *pFontItem   = m_Loader.GetFont(Font_Teeny);
	FontElement_t *pFontLabel1 = m_Loader.GetFont(Font_B);
	FontElement_t *pFontLabel2 = m_Loader.GetFont(Font_Char);

	// The spellbook screen uses a custom palette stored in ????????.IMG.
	// It's also the same palette stored in CHARSHET.COL, so we can use that
	// one until the compression scheme used for IMG files is decoded.
	m_pRaster->SetPalette(m_Loader.GetPaletteARGB(Palette_CharSheet));

	m_pRaster->DrawString(pFontValue,  11*scale,   7*scale, m_Character.Name, 254);
	m_pRaster->DrawString(pFontValue,  11*scale,  16*scale, m_Character.Race, 254);
	m_pRaster->DrawString(pFontValue,  11*scale,  25*scale, m_Character.Class, 254);
	m_pRaster->DrawNumber(pFontValue, 128*scale,  23*scale, m_Character.Level, 254);

	m_pRaster->DrawString(pFontItem, 14*scale, 50*scale, "Fireball", 83);

	// FIXME: add logic to support selecting spells, and scrolling the list

	m_pRaster->DrawBox(12*scale, 48*scale, 146*scale, 12*scale, 160);

	if (scale>1) {
		m_pRaster->DrawBox(12*scale+1, 48*scale+1, 146*scale-2, 12*scale-2, 160);
		if (scale>2) {
			m_pRaster->DrawBox(12*scale+2, 48*scale+2, 146*scale-4, 12*scale-4, 160);
		}
	}


// FIXME: remove these lines when background page can be decoded
m_pRaster->DrawString(pFontLabel2, 105*scale, 23*scale, "Level:", 253);
m_pRaster->DrawString(pFontLabel1,  63*scale, 41*scale, "Spell Book", 253);

	m_pRaster->DrawString(pFontLabel2,  14*scale, 192*scale, "Exit", 253);
	m_pRaster->DrawString(pFontLabel2,  67*scale, 191*scale, "Equipment", 253);
	m_pRaster->DrawString(pFontLabel2, 138*scale, 192*scale, "Drop", 253);

	ShowArmorRatings(scale);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderSpellPage()
//
void CwaWorld::RenderSpellPage(DWORD /*scale*/)
{
	m_pRaster->Blit(m_TextureList[Texture_SpellPage], 0, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderSpellCasting()
//
//	Displays the spell casting overlay for a particular spell effect.  There
//	are 6 different animations, each with a different number of frames and
//	different resolutions.
//
//	The frame to display is selected based upon how much time has passed
//	since the animation started.  If the time offset is large enough, the
//	animation is considered finished, the spell animation disabled.
//
void CwaWorld::RenderSpellCasting(DWORD scale)
{
	PROFILE(PFID_World_RenderSpellCasting);

	switch (m_SpellAnimation) {
		case Casting_Acid:
			{
				DWORD index = (9 * (m_CurrentTime - m_SpellCastBaseTime)) / 1200;

				if (index < 9) {
					m_pRaster->BlitTransparent(m_SpellAcid.m_pFrames[index], (160 - m_SpellAcid.m_Width) * scale, (147 - m_SpellAcid.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellAcid.m_pFrames[index], 160 * scale, (147 - m_SpellAcid.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;

		case Casting_Fire:
			{
				DWORD index = (8 * (m_CurrentTime - m_SpellCastBaseTime)) / 1000;

				if (index < 8) {
					m_pRaster->BlitTransparent(m_SpellFire.m_pFrames[index], (161 - m_SpellFire.m_Width) * scale, (147 - m_SpellFire.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellFire.m_pFrames[index], 159 * scale, (147 - m_SpellFire.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;

		case Casting_Ice:
			{
				DWORD index = (9 * (m_CurrentTime - m_SpellCastBaseTime)) / 1200;

				if (index < 9) {
					m_pRaster->BlitTransparent(m_SpellIce.m_pFrames[index], (160 - m_SpellIce.m_Width) * scale, (147 - m_SpellIce.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellIce.m_pFrames[index], 160 * scale, (147 - m_SpellIce.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;

		case Casting_Magic:
			{
				DWORD index = (14 * (m_CurrentTime - m_SpellCastBaseTime)) / 750;

				if (index < 14) {
					m_pRaster->BlitTransparent(m_SpellMagic.m_pFrames[index], (160 - m_SpellMagic.m_Width) * scale, (147 - m_SpellMagic.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellMagic.m_pFrames[index], 160 * scale, (147 - m_SpellMagic.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;

		case Casting_Shock:
			{
				DWORD index = (5 * (m_CurrentTime - m_SpellCastBaseTime)) / 750;

				if (index < 5) {
					m_pRaster->BlitTransparent(m_SpellShock.m_pFrames[index], (160 - m_SpellShock.m_Width) * scale, (147 - m_SpellShock.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellShock.m_pFrames[index], 160 * scale, (147 - m_SpellShock.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;

		case Casting_Smoke:
			{
				DWORD index = (15 * (m_CurrentTime - m_SpellCastBaseTime)) / 1000;

				if (index < 15) {
					m_pRaster->BlitTransparent(m_SpellSmoke.m_pFrames[index], (160 - m_SpellSmoke.m_Width) * scale, (147 - m_SpellSmoke.m_Height) * scale);
					m_pRaster->BlitTransparentMirror(m_SpellSmoke.m_pFrames[index], 160 * scale, (147 - m_SpellSmoke.m_Height) * scale);
				}
				else {
					m_SpellAnimation = Casting_Nothing;
				}
			}
			break;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderWater()
//
//	Draws the water (or lava) on the bottom portion of the screen.  This is
//	always blitted to the frame buffer, even if water is not visible.
//	Short of doing a test to see if all nearby tiles have floors, there is
//	no really easy way to determine whether water will be visible.
//
//	FIXME: Add logic for absysses (chasms?) which remain dark if no water
//	or lava is on the current level.
//
void CwaWorld::RenderWater(DWORD scale)
{
	PROFILE(PFID_World_RenderWater);

	// Default to displaying water this far from the top of the screen.
	long waterOffset = 100;

	// The closer the player is to water level, the more we need to push the
	// position of water upwards in the frame buffer.

	// FIXME: scale waterOffset based upon player elevation
	// this needs jumping/falling support to work

	if (m_Position[1] < c_FixedPointOne) {
		// Cannot start any higher than 46 lines down, since the top of the
		// HUD is at 146, and the water texture is only 100 lines long.
		waterOffset = 46;
	}

	if (0 != m_SkyColor) {
		DWORD color = m_SkyColor;
		if (m_UseFogMap && !m_FullBright) {
			color = 100;
		}
		m_pRaster->ClearFrameBuffer(color, waterOffset * scale);
	}

waterOffset = 74;

	// Pick a time-based frame to render.  Water and lava animations have
	// exactly 5 frames, so cycle accordingly.
	DWORD waterFrame = (5 * (m_CurrentTime % 800)) / 800;

	long waterHeight = 146 - waterOffset;
	m_pRaster->BlitRect(m_Water.m_pFrames[waterFrame], 0, waterOffset * scale,
			0, 0,
			320*scale, waterHeight*scale);

	m_pRaster->FillRect(0, waterOffset + waterHeight, 320, 200 - waterOffset - waterHeight, scale, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderWeaponOverlay()
//
//	Displays an attack animation for the current weapon.  This code is also
//	responsible for detecting whether a new attack animation has been started
//	by dragging the mouse while holding the right button.
//
//	All of the weapons have 6 attack animations, each 5 frames in length.
//	There is also a 3-frame animation for drawing/putting away the weapon.
//
//	Fists are an exception to this rule.  It only has two 5-frame animations
//	(left and right jabs), and the 3-frame ready "weapon" animation.
//
//	FIXME: adjust animations so continuous attacks work; currently need to
//	wait for current animation to complete before it will reliably trigger
//	the correct animation
//
//	FIXME: adjust the logic to also handle the fist animations
//
void CwaWorld::RenderWeaponOverlay(DWORD scale)
{
	PROFILE(PFID_World_RenderWeaponOverlay);

	if ((Attack_Off == m_AttackState) && (false == m_AttackAnimating)) {
		return;
	}

	// Default to using the frame that represents the weapon at ready pose.
	DWORD attackFrame = 32;

	// Clear this flag.  It will be set if the current animation is completed,
	// or a new animation is otherwise required.  Triggering an attack,
	// returning the weapon to the ready pose, drawing the weapon, or putting
	// the weapon away all trigger a new animation.
	//
	bool  recompute = false;

	// If there is already an attack animation in progress, continue using
	// that animation.  If the current animation is complete, switch to 
	// playing the "return to ready" animation (unless that is the animation
	// that just completed).
	if (m_AttackAnimating) {
		DWORD delta = m_CurrentTime - m_BaseAttackTime;

		if (delta > 800) {
			if (m_RightMouseDown) {
				m_AttackAnimating = false;
				m_AttackState     = Attack_Idle;
				attackFrame       = 30;
				recompute         = true;
			}
			else {
				m_AttackAnimating = true;
				m_AttackState     = Attack_Idle;
				attackFrame       = 30;
			}

			m_RightMouseX     = m_MouseX;
			m_RightMouseY     = m_MouseY;
			m_BaseAttackTime  = m_CurrentTime;
		}
		else if (Attack_Idle == m_AttackState) {
			attackFrame = 30 + (delta / 200);

			if (attackFrame > 32) {
				attackFrame = 32;
				m_AttackAnimating = false;
			}
		}
		else if (Attack_Off == m_AttackState) {
			attackFrame = 32 - (delta / 200);

			if (attackFrame < 30) {
				attackFrame = 30;
				m_AttackAnimating = false;
			}
		}
		else {
			attackFrame = delta / 200;

			switch (m_AttackState) {
				case Attack_Thrust:      attackFrame += 25; break;
				case Attack_Chop:        attackFrame +=  0; break;
				case Attack_DownToRight: attackFrame += 20; break;
				case Attack_LeftToRight: attackFrame += 15; break;
				case Attack_RightToLeft: attackFrame += 10; break;
				case Attack_DownToLeft:  attackFrame +=  5; break;
			}
		}
	}
	else {
		recompute = true;
	}

	// If an animation has completed, and a right drag is occurring,
	// decide whether we can trigger a new attack animation.
	if (recompute && m_RightMouseDown && (Attack_Off != m_AttackState)) {
		DWORD delta = m_CurrentTime - m_BaseAttackTime;

		// Wait at least 75 milliseconds for the player to drag in some
		// direction.
		if (delta > 75) {
			// What direction is the mouse moving, and by how much.
			// Based upon this, we'll pick which animation to display.
			long dx = long(m_RightMouseX) - long(m_MouseX);
			long dy = long(m_RightMouseY) - long(m_MouseY);
			long magx = abs(dx);

			bool wasAnimating = m_AttackAnimating;

			m_AttackAnimating = true;

			if ((magx <= 10) && (dy > 10)) {
				m_AttackState = Attack_Thrust;
			}
			else if ((magx <= 10) && (dy < -10)) {
				m_AttackState = Attack_Chop;
			}
			else if (dx < -10) {
				if (dy < (dx / 4)) {
					m_AttackState = Attack_DownToRight;
				}
				else {
					m_AttackState = Attack_LeftToRight;
				}
			}
			else if (dx > 10) {
				if (dy < (dx / -4)) {
					m_AttackState = Attack_DownToLeft;
				}
				else {
					m_AttackState = Attack_RightToLeft;
				}
			}

			else {
				m_AttackAnimating = false;
			}

			// If a new attack animation has been triggered, record the base
			// time of the animation, and possibly play a sound to indicate
			// that a swing has occurred.
			if (m_AttackAnimating) {
				m_BaseAttackTime = m_CurrentTime;

				if (false == wasAnimating) {
					DWORD id;
					if (CheckHitCritterMelee(id)) {
						if (Random32(100) < 50) {
							sprintf(m_MessageBuffer, "Hit %s", m_Critter[id].Name);
							SetDisplayMessage();
							m_Critter[id].State    = CritterState_Dying;
							m_Critter[id].BaseTime = m_CurrentTime;
							m_SoundDriver.QueueSound(m_SoundClips[Sound_UHit].SoundID);
						}
						else {
							m_SoundDriver.QueueSound(m_SoundClips[Sound_Clank].SoundID);
						}
					}
					else {
						m_SoundDriver.QueueSound(m_SoundClips[Sound_Umph].SoundID);
					}
				}
			}
		}
	}

	CwaTexture &weapon = m_pWeapon->m_pFrames[attackFrame];

//	CwaTexture &weapon = m_WeaponAxe.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponHammer.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponHand.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponMace.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponStaff.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponStar.m_pFrames[attackFrame];
//	CwaTexture &weapon = m_WeaponSword.m_pFrames[attackFrame];

	m_pRaster->BlitTransparent(weapon, weapon.m_OffsetX*scale, weapon.m_OffsetY*scale);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderCell()
//
void CwaWorld::RenderCell(GridCoords_t *pGrid0, GridCoords_t *pGrid1, DWORD south, DWORD east)
{
	PROFILE(PFID_World_RenderCell);

	Quad_t quad;

	DWORD playerEast  = m_Position[0] >> c_FixedPointBits;
	DWORD playerLevel = m_Position[1] >> c_FixedPointBits;
	DWORD playerSouth = m_Position[2] >> c_FixedPointBits;

	GridCoords_t g0, g1, g2, g3;
	BlockSide_t  blockSide;
	memset(&blockSide, 0, sizeof(blockSide));

	CellNode_t &node = m_Nodes[south][east];

	blockSide.Flags = node.Flags;

	for (DWORD level = 0; level < 3; ++level) {

			CwaTexture *pTexture = &m_WallList[node.Texture[level]];
					if (0 == node.Texture[level]) {
						pTexture = &m_WallList[1];
					}


		if ((level > 0) && ((CellFlag_Solid_L0 << level) & node.Flags)) {

			///////////////////////
			//  Bottom of block  //
			///////////////////////

			long  bottomLevel = c_FloorHeight + (c_WallHeight * (long(level) - 1));
			DWORD flags       = 0;

			if ((2 == level) && (CellFlag_SecretDoor & node.Flags)) {
				Trigger_t &trigger = m_Triggers[node.Trigger & 0xFF];

				long angle = PortalAngle(trigger);

				bottomLevel = c_FixedPointOne + FixedMul(c_WallHeight, angle);
			}
			else if ((2 == level) && (CellFlag_Door & node.Flags)) {
				// no-op
			}
			else if (0 == ((CellFlag_Solid_L0 << (level-1)) & node.Flags)) {
				// no-op
			}
			else {
				bottomLevel = 0;
			}

			if (m_Position[1] < bottomLevel) {
				quad.Corners[0].x = pGrid1[east  ].x;
				quad.Corners[1].x = pGrid1[east+1].x;
				quad.Corners[2].x = pGrid0[east+1].x;
				quad.Corners[3].x = pGrid0[east  ].x;
				quad.Corners[0].y = bottomLevel - m_Position[1];
				quad.Corners[1].y = bottomLevel - m_Position[1];
				quad.Corners[2].y = bottomLevel - m_Position[1];
				quad.Corners[3].y = bottomLevel - m_Position[1];
				quad.Corners[0].z = pGrid1[east  ].z;
				quad.Corners[1].z = pGrid1[east+1].z;
				quad.Corners[2].z = pGrid0[east+1].z;
				quad.Corners[3].z = pGrid0[east  ].z;
				quad.Corners[0].l = pGrid1[east  ].luma;
				quad.Corners[1].l = pGrid1[east+1].luma;
				quad.Corners[2].l = pGrid0[east+1].luma;
				quad.Corners[3].l = pGrid0[east  ].luma;

				if (1 == level) {
					AddHorizontalQuad(quad, &m_TextureList[Texture_TrenchCeiling], 0, flags);
				}
				else {
					AddHorizontalQuad(quad, pTexture, 0, flags);
				}
			}
		}

		if ((level < 2) && ((CellFlag_Solid_L0 << level) & node.Flags)) {

			////////////////////
			//  Top of block  //
			////////////////////

			long topLevel = c_FloorHeight + (c_WallHeight * level);

			if ((0 == level) && (CellFlag_Raised & m_Nodes[south][east].Flags)) {
				topLevel += c_WallHeight >> 2;
			}
			else if ((0 == level) && ((CellFlag_Door | CellFlag_SecretDoor) & node.Flags)) {
				// no-op
			}
			else if (0 == ((CellFlag_Solid_L0 << (level+1)) & node.Flags)) {
				// no-op
			}
			else {
				// Pick a big number so next test will fail.
				topLevel = IntToFixed(4);
			}

			if (m_Position[1] > topLevel) {
				quad.Corners[0].x = pGrid0[east  ].x;
				quad.Corners[1].x = pGrid0[east+1].x;
				quad.Corners[2].x = pGrid1[east+1].x;
				quad.Corners[3].x = pGrid1[east  ].x;
				quad.Corners[0].y = topLevel - m_Position[1];
				quad.Corners[1].y = topLevel - m_Position[1];
				quad.Corners[2].y = topLevel - m_Position[1];
				quad.Corners[3].y = topLevel - m_Position[1];
				quad.Corners[0].z = pGrid0[east  ].z;
				quad.Corners[1].z = pGrid0[east+1].z;
				quad.Corners[2].z = pGrid1[east+1].z;
				quad.Corners[3].z = pGrid1[east  ].z;
				quad.Corners[0].l = pGrid0[east  ].luma;
				quad.Corners[1].l = pGrid0[east+1].luma;
				quad.Corners[2].l = pGrid1[east+1].luma;
				quad.Corners[3].l = pGrid1[east  ].luma;

				AddHorizontalQuad(quad, pTexture);
			}
		}

		const DWORD testMask = CellFlag_Raised | CellFlag_Door | CellFlag_SecretDoor;

		if (((CellFlag_Solid_L0 << level) & node.Flags) || ((1 == level) && (testMask & node.Flags))) {
			if ((1 == level) && (CellFlag_Door & node.Flags)) {
				// Normal doors swing away from the player.  Use the state of
				// the door and the player's position to compute an angle.

				// Door opens east/west
				if (((east > 1)           && (0 == (CellFlag_Solid_L1 & m_Nodes[south][east-1].Flags))) ||
					((east < m_MaxEast-1) && (0 == (CellFlag_Solid_L1 & m_Nodes[south][east+1].Flags))))
				{
					// Player is facing east
					if ( (playerEast  < east) ||
						((playerEast == east) && (m_Direction < c_FixedPointHalf)))
					{
						g0 = pGrid0[east  ];
						g1 = pGrid1[east  ];
					}

					// Player is facing west
					else {
						g0 = pGrid1[east+1];
						g1 = pGrid0[east+1];
					}
				}

				// Door opens north/south
				else {
					// Player is facing north
					if ( (playerSouth  > south) ||
						((playerSouth == south) && ((m_Direction < (c_FixedPointOne/4)) || (m_Direction > ((3*c_FixedPointOne)/4)))))
					{
						g0 = pGrid1[east  ];
						g1 = pGrid1[east+1];
					}

					// Player is facing south
					else {
						g0 = pGrid0[east+1];
						g1 = pGrid0[east  ];
					}
				}

				// Check the state of the door's trigger.  This will
				// determine the angle used to rotate the door away from
				// the player.

				Trigger_t &trigger = m_Triggers[node.Trigger & 0xFF];

				long angle = PortalAngle(trigger);

				long dx = g1.x - g0.x;
				long dz = g1.z - g0.z;

				long as = FloatToFixed(sinf(float(angle) * 3.14159f / float(2 * c_FixedPointOne)));
				long ac = FloatToFixed(cosf(float(angle) * 3.14159f / float(2 * c_FixedPointOne)));

				long dxp = FixedMul(dx, ac) - FixedMul(dz, as);
				long dzp = FixedMul(dx, as) + FixedMul(dz, ac);

				g1.x = g0.x + dxp;
				g1.z = g0.z + dzp;

				quad.Corners[0].x = g0.x;
				quad.Corners[1].x = g1.x;
				quad.Corners[2].x = g1.x;
				quad.Corners[3].x = g0.x;
				quad.Corners[0].y = c_FloorHeight + c_WallHeight - m_Position[1];
				quad.Corners[1].y = c_FloorHeight + c_WallHeight - m_Position[1];
				quad.Corners[2].y = c_FloorHeight - m_Position[1];
				quad.Corners[3].y = c_FloorHeight - m_Position[1];
				quad.Corners[0].z = g0.z;
				quad.Corners[1].z = g1.z;
				quad.Corners[2].z = g1.z;
				quad.Corners[3].z = g0.z;
				quad.Corners[0].l = g0.luma;
				quad.Corners[1].l = g1.luma;
				quad.Corners[2].l = g1.luma;
				quad.Corners[3].l = g0.luma;
				quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
				quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
				quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
				quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
				quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
				quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
				quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
				quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

				AddWallQuad(quad, pTexture, CellTrigger(south, east), QuadFlag_ZBiasCloser | QuadFlag_Transparent);
			}
			else {
				DWORD flags         = 0;
				long  verticalShift = c_FloorHeight + (c_WallHeight * (long(level) - 1));

				if (playerLevel != level) {
//					flags    |= QuadFlag_ZBiasFarther;
				}

				if (0 == level) {
					flags    |= QuadFlag_Transparent;
					pTexture  = &m_TextureList[Texture_Abyss];
				}

				if (CellFlag_Transparent & node.Flags) {
					flags    |= QuadFlag_Transparent;
				}

				for (DWORD wallNum = 0; wallNum < 4; ++wallNum) {
					bool showBlock = false;

					// North face
					if (0 == wallNum) {
						if ((playerSouth < south) && ShowOutsideOfBlock(south-1, east, level, blockSide)) {
							showBlock = true;
							g0 = pGrid0[east+1];
							g1 = pGrid0[east  ];
							g2 = pGrid0[east  ];
							g3 = pGrid0[east+1];
						}
					}

					// South face
					else if (1 == wallNum) {
						if ((playerSouth > south) && ShowOutsideOfBlock(south+1, east, level, blockSide)) {
							showBlock = true;
							g0 = pGrid1[east  ];
							g1 = pGrid1[east+1];
							g2 = pGrid1[east+1];
							g3 = pGrid1[east  ];
						}
					}

					// East face
					else if (2 == wallNum) {
						if ((playerEast > east) && ShowOutsideOfBlock(south, east+1, level, blockSide)) {
							showBlock = true;
							g0 = pGrid1[east+1];
							g1 = pGrid0[east+1];
							g2 = pGrid0[east+1];
							g3 = pGrid1[east+1];
						}
					}

					// West face
					else {
						if ((playerEast < east) && ShowOutsideOfBlock(south, east-1, level, blockSide)) {
							showBlock = true;
							g0 = pGrid0[east  ];
							g1 = pGrid1[east  ];
							g2 = pGrid1[east  ];
							g3 = pGrid0[east  ];
						}
					}

					if (showBlock) {
						DWORD shapeID = 0;

						if (1 == level) {
							if (CellFlag_SecretDoor & node.Flags) {
								Trigger_t &trigger = m_Triggers[node.Trigger & 0xFF];

								long angle = PortalAngle(trigger);
								long shift = FixedMul(c_WallHeight, angle);

								blockSide.yHi = c_WallHeight;
								blockSide.yLo = shift;
								blockSide.vHi = angle;
								blockSide.vLo = c_FixedPointOne; 

								shapeID = CellTrigger(south, east);
							}
							else if (CellFlag_Portal & node.Flags) {
								shapeID = CellTrigger(south, east);
							}
						}

						quad.Corners[0].x = g0.x;
						quad.Corners[1].x = g1.x;
						quad.Corners[2].x = g2.x;
						quad.Corners[3].x = g3.x;
						quad.Corners[0].y = blockSide.yHi + verticalShift - m_Position[1];
						quad.Corners[1].y = blockSide.yHi + verticalShift - m_Position[1];
						quad.Corners[2].y = blockSide.yLo + verticalShift - m_Position[1];
						quad.Corners[3].y = blockSide.yLo + verticalShift - m_Position[1];
						quad.Corners[0].z = g0.z;
						quad.Corners[1].z = g1.z;
						quad.Corners[2].z = g2.z;
						quad.Corners[3].z = g3.z;
						quad.Corners[0].l = g0.luma;
						quad.Corners[1].l = g1.luma;
						quad.Corners[2].l = g2.luma;
						quad.Corners[3].l = g3.luma;
						quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
						quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
						quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
						quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
						quad.Corners[0].v = blockSide.vHi;
						quad.Corners[1].v = blockSide.vHi;
						quad.Corners[2].v = blockSide.vLo;
						quad.Corners[3].v = blockSide.vLo;

						AddWallQuad(quad, pTexture, shapeID, flags);
					}
				}
			}
		}
		else if ((1 == level) && (CellFlag_SignMask & node.Flags)) {
			if (CellFlag_SignNorth & node.Flags) {
				if (playerSouth < south) {
					g0 = pGrid0[east+1];
					g1 = pGrid0[east  ];
				}
				else {
					g0 = pGrid0[east  ];
					g1 = pGrid0[east+1];
				}
			}
			else if (CellFlag_SignSouth & node.Flags) {
				if (playerSouth <= south) {
					g0 = pGrid1[east+1];
					g1 = pGrid1[east  ];
				}
				else {
					g0 = pGrid1[east  ];
					g1 = pGrid1[east+1];
				}
			}
			else if (CellFlag_SignEast & node.Flags) {
				if (playerEast <= east) {
					g0 = pGrid0[east+1];
					g1 = pGrid1[east+1];
				}
				else {
					g0 = pGrid1[east+1];
					g1 = pGrid0[east+1];
				}
			}
			else {
				if (playerEast < east) {
					g0 = pGrid0[east];
					g1 = pGrid1[east];
				}
				else {
					g0 = pGrid1[east];
					g1 = pGrid0[east];
				}
			}

			long verticalShift = c_FloorHeight - m_Position[1];

			verticalShift += ((node.Flags & CellFlag_SignShift) >> 16) * (c_WallHeight / 4);

			quad.Corners[0].x = g0.x;
			quad.Corners[1].x = g1.x;
			quad.Corners[2].x = g1.x;
			quad.Corners[3].x = g0.x;
			quad.Corners[0].y = verticalShift + c_WallHeight;
			quad.Corners[1].y = verticalShift + c_WallHeight;
			quad.Corners[2].y = verticalShift;
			quad.Corners[3].y = verticalShift;
			quad.Corners[0].z = g0.z;
			quad.Corners[1].z = g1.z;
			quad.Corners[2].z = g1.z;
			quad.Corners[3].z = g0.z;
			quad.Corners[0].l = g0.luma;
			quad.Corners[1].l = g1.luma;
			quad.Corners[2].l = g1.luma;
			quad.Corners[3].l = g0.luma;
			quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
			quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
			quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
			quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
			quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
			quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

			AddWallQuad(quad, pTexture, 0, QuadFlag_ZBiasCloser | QuadFlag_Transparent);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderTriQuad()
//
//	Given 4 vertices defining the corners of a quad, it will split the quad
//	into two triangles and render them 
//
void CwaWorld::RenderTriQuad(Vertex_t corners[], DWORD shapeID, bool transparent)
{
	ResetTriangleQueues();

	corners[0].u =  0 << (c_FixedPointBits - 6);
	corners[1].u = 64 << (c_FixedPointBits - 6);
	corners[2].u = 64 << (c_FixedPointBits - 6);
	corners[3].u =  0 << (c_FixedPointBits - 6);
	corners[0].v =  0 << (c_FixedPointBits - 6);
	corners[1].v =  0 << (c_FixedPointBits - 6);
	corners[2].v = 64 << (c_FixedPointBits - 6);
	corners[3].v = 64 << (c_FixedPointBits - 6);

	for (DWORD i = 0; i < 4; ++i) {
		corners[i].l  = ComputeBrightness(&corners[i].x);

		corners[i].x -= m_Position[0];
		corners[i].y -= m_Position[1];
		corners[i].z  = m_Position[2] - corners[i].z;

		long x = corners[i].x;
		corners[i].x = FixedMul(x, m_DirectionCosine) - FixedMul(corners[i].z, m_DirectionSine);
		corners[i].z = FixedMul(x, m_DirectionSine)   + FixedMul(corners[i].z, m_DirectionCosine);
	}

	Triangle_t *pTri1 = NewTriangle();
	Triangle_t *pTri2 = NewTriangle();

	pTri1->Corners[0] = corners[0];
	pTri1->Corners[1] = corners[1];
	pTri1->Corners[2] = corners[2];

	pTri2->Corners[0] = corners[0];
	pTri2->Corners[1] = corners[2];
	pTri2->Corners[2] = corners[3];

	PushOutputTriangle(pTri1);
	PushOutputTriangle(pTri2);

	ClipNearZ();

	RenderTriangleList(transparent, shapeID);
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderSprite()
//
void CwaWorld::RenderSprite(DWORD spriteNum)
{
	PROFILE(PFID_World_RenderSprite);

	Sprite_t &sprite = m_Sprite[spriteNum];

	long xp = sprite.Position[0] - m_Position[0];
	long zp = m_Position[2] - sprite.Position[2];

	long x = FixedMul(xp, m_DirectionCosine) - FixedMul(zp, m_DirectionSine);
	long y = sprite.Position[1] - m_Position[1];
	long z = FixedMul(xp, m_DirectionSine)   + FixedMul(zp, m_DirectionCosine);

	// Simple frustum culling.
	if ((z < c_ZNear) || (abs(x) > (z + IntToFixed(2)))) {
		return;
	}

	// If the sprite is too far away, don't draw it.
	if ((abs(x >> c_FixedPointBits) > long(m_MaxDrawDistance)) ||
		(abs(z >> c_FixedPointBits) > long(m_MaxDrawDistance)))
	{
		return;
	}

	CwaTexture *pTexture = NULL;

	if (SpriteFlag_Animated & sprite.Flags) {
		CwaAnimation &animation = m_Animations[sprite.TextureID];
		DWORD duration = g_AnimInfo[sprite.TextureID].LoopDuration;

		DWORD frameNum = (animation.m_FrameCount * (m_CurrentTime % duration)) / duration;

		pTexture = &(animation.m_pFrames[frameNum]);
	}
	else {
		pTexture = &m_FlatList[sprite.TextureID];
	}

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quad = m_QuadList[m_QuadCount];

		quad.Corners[0].x = x - (sprite.Width >> 1);
		quad.Corners[1].x = x + (sprite.Width >> 1);
		quad.Corners[2].x = x + (sprite.Width >> 1);
		quad.Corners[3].x = x - (sprite.Width >> 1);
		quad.Corners[0].y = y + sprite.Height;
		quad.Corners[1].y = y + sprite.Height;
		quad.Corners[2].y = y;
		quad.Corners[3].y = y;
		quad.Corners[0].z = z;
		quad.Corners[1].z = z;
		quad.Corners[2].z = z;
		quad.Corners[3].z = z;

		quad.Depth = FixedMul(x, x) + FixedMul(y, y) + FixedMul(z, z);

		if (TransformVertices(quad.Corners, 4)) {
			long luma = ComputeBrightness(sprite.Position);

			quad.Corners[0].l = luma;
			quad.Corners[1].l = luma;
			quad.Corners[2].l = luma;
			quad.Corners[3].l = luma;

			quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
			quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
			quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
			quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
			quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
			quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

			quad.ShapeID  = c_Shape_Sprite | spriteNum;
			quad.Flags    = QuadFlag_Sprite | 4;
			quad.pTexture = pTexture;

			++m_QuadCount;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderProjectile()
//
void CwaWorld::RenderProjectile(DWORD projectileNum)
{
	PROFILE(PFID_World_RenderProjectile);

	Projectile_t &proj = m_Projectile[projectileNum];

	if (1 == proj.Active) {
		long newX = proj.Position[0] + (proj.DeltaX * long(m_CurrentTimeDelta)) / 1000;
		long newZ = proj.Position[2] + (proj.DeltaZ * long(m_CurrentTimeDelta)) / 1000;

		long east  = newX >> c_FixedPointBits;
		long south = newZ >> c_FixedPointBits;

		// Just a dummy var for CheckCritterCollision().
		DWORD id = DWORD(-1);

		if (IsBlockSolid(south, east, 1) || CheckCritterCollision(newX, newZ, id)) {
			newX = proj.Position[0] - (proj.DeltaX >> 4);
			newZ = proj.Position[2] - (proj.DeltaZ >> 4);

			proj.Active   = 2;
			proj.BaseTime = m_CurrentTime;

			long distance = FixedDistance(proj.Position[0] - m_Position[0], proj.Position[2] - m_Position[2]);

			// Scale volume so explosions more than 16 (2^4) cells away will
			// be silent.
			long volume = 256 - (distance >> (c_FixedPointBits - 4));

			if (volume > 0) {
				m_SoundDriver.QueueSound(proj.ExplodeSound, 1, volume);
			}

			// If the point of collision was against a moster, the projectile
			// will have some kind of effect on the monster.
			if (DWORD(-1) != id) {
				sprintf(m_MessageBuffer, "Hit %s", m_Critter[id].Name);
				SetDisplayMessage();
				m_Critter[id].State    = CritterState_Dying;
				m_Critter[id].BaseTime = m_CurrentTime;
			}
		}

		proj.Position[0] = newX;
		proj.Position[2] = newZ;
	}

	long xp = proj.Position[0] - m_Position[0];
	long zp = m_Position[2] - proj.Position[2];

	long x = FixedMul(xp, m_DirectionCosine) - FixedMul(zp, m_DirectionSine);
	long y = proj.Position[1] - m_Position[1];
	long z = FixedMul(xp, m_DirectionSine)   + FixedMul(zp, m_DirectionCosine);

	DWORD animID       = proj.BulletID;
	DWORD milliseconds = m_CurrentTime - proj.BaseTime;

	if (2 == proj.Active) {
		animID = proj.ExplodeID;

		if (milliseconds >= g_AnimInfo[animID].LoopDuration) {
			proj.Active = 0;
			return;
		}

		DWORD ramp = g_AnimInfo[animID].LoopDuration >> 2;

		if (milliseconds < ramp) {
			DWORD lo  = g_AnimInfo[proj.BulletID].Brightness;
			DWORD hi  = g_AnimInfo[proj.ExplodeID].Brightness;
			DWORD mix = lo + (((hi - lo) * milliseconds) / ramp);
			proj.Brightness = (mix * c_FixedPointMask) / 100;
		}
		else if (milliseconds > (g_AnimInfo[animID].LoopDuration - ramp)) {
			DWORD fade = g_AnimInfo[animID].LoopDuration - milliseconds;

			proj.Brightness = (g_AnimInfo[animID].Brightness * c_FixedPointMask) / 100;

			proj.Brightness = (proj.Brightness * fade) / ramp;
		}
		else {
			proj.Brightness = (g_AnimInfo[animID].Brightness * c_FixedPointMask) / 100;
		}
	}
	else {
		milliseconds = milliseconds % g_AnimInfo[animID].LoopDuration;
	}

	if (z < c_ZNear) {
		return;
	}

	CwaAnimation &animation = m_Animations[animID];

	DWORD frameNum = (animation.m_FrameCount * milliseconds) / g_AnimInfo[animID].LoopDuration;

	long luma   = c_MaxLuma;
	long width  = IntToFixed(m_Animations[animID].m_Width)  / g_AnimInfo[animID].Scale;
	long height = IntToFixed(m_Animations[animID].m_Height) / g_AnimInfo[animID].Scale;

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quad = m_QuadList[m_QuadCount];

		quad.Corners[0].x = x - width;
		quad.Corners[1].x = x + width;
		quad.Corners[2].x = x + width;
		quad.Corners[3].x = x - width;
		quad.Corners[0].y = y + height;
		quad.Corners[1].y = y + height;
		quad.Corners[2].y = y - height;
		quad.Corners[3].y = y - height;
		quad.Corners[0].z = z;
		quad.Corners[1].z = z;
		quad.Corners[2].z = z;
		quad.Corners[3].z = z;
		quad.Corners[0].l = luma;
		quad.Corners[1].l = luma;
		quad.Corners[2].l = luma;
		quad.Corners[3].l = luma;
		quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
		quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
		quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
		quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
		quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
		quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
		quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
		quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

		quad.Depth = VertexDepth(quad.Corners[0]);

		if (TransformVertices(quad.Corners, 4)) {
			quad.ShapeID  = 0;
			quad.Flags    = QuadFlag_Sprite | 4;// | QuadFlag_ZBiasCloser;
			quad.pTexture = &(animation.m_pFrames[frameNum]);

			++m_QuadCount;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderCritter()
//
void CwaWorld::RenderCritter(DWORD critterNum)
{
	PROFILE(PFID_World_RenderCritter);

	Critter_t &critter = m_Critter[critterNum];

	long xp = critter.Position[0] - m_Position[0];
	long zp = m_Position[2] - critter.Position[2];

	long x = FixedMul(xp, m_DirectionCosine) - FixedMul(zp, m_DirectionSine);
	long y = critter.Position[1] - m_Position[1];
	long z = FixedMul(xp, m_DirectionSine)   + FixedMul(zp, m_DirectionCosine);

	if ((critter.State >= CritterState_Alive) && ((m_CurrentTime - critter.VocalizeTime) < 30000)) {
		long distance = FixedDistance(xp, zp);
		if (distance < IntToFixed(8)) {
			long volume = 256 - (distance >> (c_FixedPointBits - 5));

			if (volume > 0) {
				m_SoundDriver.QueueSound(g_CritterInfo[critter.CritterID].VocalizeID, 1, volume);
			}
		}

		critter.VocalizeTime = m_CurrentTime + 10000 + (rand() % 20000);
	}

	if (z < c_ZNear) {
		return;
	}

	// Test code: makes the critter spin in place to test orientation
//	critter.Direction = (critter.Direction + 64) & c_FixedPointMask;

	CwaCreature &creature = m_Creatures[critter.CritterID];

	DWORD frameNum = 0;
	DWORD orient   = 0;
	bool  mirror   = false;

	if (critter.State >= CritterState_Alive) {
		DWORD frameCount = creature.m_Animation[1].m_FrameCount;

		frameNum = (frameCount * (m_CurrentTime % 1800)) / 1800;

		const float inv = 1.0f / float(c_FixedPointOne);

		float anglef = atan2f(float(xp) * inv, float(zp) * inv) * (0.5f / 3.14159f);
		long  angle  = long(anglef * c_FixedPointOne) + c_FixedPointHalf;

		orient = ((((critter.Direction + angle) + (c_FixedPointHalf >> 3)) >> (c_FixedPointBits - 3)) & 0x7);
		mirror = false;

		if (orient > 4) {
			orient = 8 - orient;
			mirror = true;
		}
	}
	else {
		if (CritterState_Dying == critter.State) {
			frameNum = (m_CurrentTime - critter.BaseTime) / 180;
			if (frameNum > 2) {
				if (CritterFlag_CorpsePersists & g_CritterInfo[critter.CritterID].Flags) {
					frameNum = 2;
					critter.State = CritterState_Dead;
				}
				else {
					critter.State = CritterState_FreeSlot;
					return;
				}
			}
		}
		else {
			frameNum = 2;
		}

		orient = 5;
	}

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quad = m_QuadList[m_QuadCount];

		long width  = IntToFixed(creature.m_Animation[orient].m_Width)  / g_CritterInfo[critter.CritterID].Scale[orient];
		long height = IntToFixed(creature.m_Animation[orient].m_Height) / g_CritterInfo[critter.CritterID].Scale[orient];
		long center = IntToFixed(g_CritterInfo[critter.CritterID].Center[orient])   / g_CritterInfo[critter.CritterID].Scale[orient];
		long vdisp  = IntToFixed(g_CritterInfo[critter.CritterID].Vertical[orient]) / g_CritterInfo[critter.CritterID].Scale[orient];

		if (mirror) {
			center = IntToFixed(creature.m_Animation[orient].m_Width - g_CritterInfo[critter.CritterID].Center[orient]) / g_CritterInfo[critter.CritterID].Scale[orient];
		}

		quad.Corners[0].x = x - center;
		quad.Corners[1].x = x + width - center;
		quad.Corners[2].x = x + width - center;
		quad.Corners[3].x = x - center;
		quad.Corners[0].y = y + vdisp + height;
		quad.Corners[1].y = y + vdisp + height;
		quad.Corners[2].y = y + vdisp;
		quad.Corners[3].y = y + vdisp;
		quad.Corners[0].z = z;
		quad.Corners[1].z = z;
		quad.Corners[2].z = z;
		quad.Corners[3].z = z;

		quad.Depth = FixedMul(x, x) + FixedMul(y, y) + FixedMul(z, z);

		if (TransformVertices(quad.Corners, 4)) {
			long luma = ComputeBrightness(critter.Position);

			quad.Corners[0].l = luma;
			quad.Corners[1].l = luma;
			quad.Corners[2].l = luma;
			quad.Corners[3].l = luma;

			if (false == mirror) {
				quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
				quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
				quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
				quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
			}
			else {
				quad.Corners[0].u = 64 << (c_FixedPointBits - 6);
				quad.Corners[1].u =  0 << (c_FixedPointBits - 6);
				quad.Corners[2].u =  0 << (c_FixedPointBits - 6);
				quad.Corners[3].u = 64 << (c_FixedPointBits - 6);
			}

			quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
			quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
			quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

			quad.ShapeID  = c_Shape_Critter | critterNum;
			quad.Flags    = QuadFlag_Sprite | 4; //| QuadFlag_ZBiasCloser;
			quad.pTexture = &(creature.m_Animation[orient].m_pFrames[frameNum]);

			if (0 != (CritterFlag_Translucent & g_CritterInfo[critter.CritterID].Flags)) {
				quad.Flags |= QuadFlag_Translucent;
			}

			++m_QuadCount;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderTriangleList()
//
//	Given a list of triangles, each is transformed into screen space, then
//	rendered into the frame buffer.
//
void CwaWorld::RenderTriangleList(bool transparent, DWORD shapeID)
{
	SwapInputOutputTriangleQueues();

	Triangle_t *pInput;

	while (NULL != (pInput = PopInputTriangle())) {

		// Transform the vertices of this triangle.  Only try to render it if
		// TransformVertices() returns true -- otherwise we're dealing with a
		// triangle that is definitely not visible, so we can skip rendering
		// and save a little processing time.
		if (TransformVertices(pInput->Corners, 3)) {
			if (false == transparent) {
				if (pInput->Corners[0].z < c_PerspectiveAdjustLimit) {
					m_pRaster->DrawTriangle(&pInput->Corners[0], &pInput->Corners[1], &pInput->Corners[2], shapeID);
				}
				else {
					m_pRaster->DrawTrianglePerspective(&pInput->Corners[0], &pInput->Corners[1], &pInput->Corners[2], shapeID);
				}
			}
			else {
				m_pRaster->DrawTriangleTransparent(&pInput->Corners[0], &pInput->Corners[1], &pInput->Corners[2], shapeID);
			}
		}

		FreeTriangle(pInput);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	CreateHorizontalQuad()
//
bool CwaWorld::CreateHorizontalQuad(Quad_t &quad)
{
	if ((quad.Corners[0].z < c_ZNear) &&
		(quad.Corners[1].z < c_ZNear) &&
		(quad.Corners[2].z < c_ZNear) &&
		(quad.Corners[3].z < c_ZNear))
	{
		return false;
	}
	quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
	quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
	quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
	quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
	quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
	quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
	quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
	quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

	quad.Depth = VertexDepth(quad.Corners[0]);
	for (DWORD j = 1; j < 4; ++j) {
		long depth = VertexDepth(quad.Corners[j]);

		if (quad.Depth < depth) {
			quad.Depth = depth;
		}
	}

	return ClipQuadToNearZ(quad);
}


/////////////////////////////////////////////////////////////////////////////
//
//	AddHorizontalQuad()
//
void CwaWorld::AddHorizontalQuad(Quad_t &quad, CwaTexture *pTexture, DWORD shapeID, DWORD flags)
{
	PROFILE(PFID_World_AddHorizontalQuad);

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quadOut = m_QuadList[m_QuadCount];

		if (CreateHorizontalQuad(quad)) {
			quadOut = quad;

			quadOut.ShapeID  = shapeID;
			quadOut.Flags   |= flags | QuadFlag_Ngon;
			quadOut.pTexture = pTexture;

			++m_QuadCount;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////
//
//	AddWallQuad()
//
void CwaWorld::AddWallQuad(Quad_t &quadIn, CwaTexture *pTexture, DWORD shapeID, DWORD flags)
{
	PROFILE(PFID_World_AddVerticalQuad);

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quad = m_QuadList[m_QuadCount];

		// Quad faces away from the camera.
//		if (quadIn.Corners[0].x > quadIn.Corners[1].x) {
//			return;
//		}

		// Quad is completely behind the near Z plane.
		if ((quadIn.Corners[0].z < c_ZNear) &&
			(quadIn.Corners[1].z < c_ZNear))
		{
			return;
		}

		quad = quadIn;

		quad.Depth = VertexDepth(quad.Corners[0]);
		for (DWORD i = 1; i < 4; ++i) {
			long depth = VertexDepth(quad.Corners[i]);
			if (quad.Depth < depth) {
				quad.Depth = depth;
			}
		}

		if (quad.Corners[0].z < c_ZNear) {
			long frac = FixedDiv((c_ZNear - quad.Corners[0].z), (quad.Corners[1].z - quad.Corners[0].z));

			LerpVertex(quad.Corners[0], quad.Corners[1], quad.Corners[0], frac);
			LerpVertex(quad.Corners[3], quad.Corners[2], quad.Corners[3], frac);
		}
		else if (quad.Corners[1].z < c_ZNear) {
			long frac = FixedDiv((c_ZNear - quad.Corners[1].z), (quad.Corners[0].z - quad.Corners[1].z));

			LerpVertex(quad.Corners[1], quad.Corners[0], quad.Corners[1], frac);
			LerpVertex(quad.Corners[2], quad.Corners[3], quad.Corners[2], frac);
		}

		if (TransformVertices(quad.Corners, 4)) {
			quad.ShapeID  = shapeID;
			quad.Flags    = flags | 4;
			quad.pTexture = pTexture;

			++m_QuadCount;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	AddVerticalQuad()
//
void CwaWorld::AddVerticalQuad(Vertex_t corners[], CwaTexture *pTexture, DWORD shapeID, DWORD flags)
{
	PROFILE(PFID_World_AddVerticalQuad);

	if (m_QuadCount < ArraySize(m_QuadList)) {
		Quad_t &quad = m_QuadList[m_QuadCount];

		for (DWORD i = 0; i < 4; ++i) {
			quad.Corners[i].x = corners[i].x - m_Position[0];
			quad.Corners[i].y = corners[i].y - m_Position[1];
			quad.Corners[i].z = m_Position[2] - corners[i].z;

			long x = quad.Corners[i].x;
			quad.Corners[i].x = FixedMul(x, m_DirectionCosine) - FixedMul(quad.Corners[i].z, m_DirectionSine);
			quad.Corners[i].z = FixedMul(x, m_DirectionSine)   + FixedMul(quad.Corners[i].z, m_DirectionCosine);
		}

		// Quad faces away from the camera.
		if (quad.Corners[0].x > quad.Corners[1].x) {
			return;
		}

		// Quad is completely behind the near Z plane.
		if ((quad.Corners[0].z < c_ZNear) &&
			(quad.Corners[1].z < c_ZNear))
		{
			return;
		}

		quad.Corners[0].l = quad.Corners[3].l = ComputeBrightness(&corners[0].x);
		quad.Corners[1].l = quad.Corners[2].l = ComputeBrightness(&corners[1].x);

		quad.Corners[0].u =  0 << (c_FixedPointBits - 6);
		quad.Corners[1].u = 64 << (c_FixedPointBits - 6);
		quad.Corners[2].u = 64 << (c_FixedPointBits - 6);
		quad.Corners[3].u =  0 << (c_FixedPointBits - 6);
		quad.Corners[0].v =  0 << (c_FixedPointBits - 6);
		quad.Corners[1].v =  0 << (c_FixedPointBits - 6);
		quad.Corners[2].v = 64 << (c_FixedPointBits - 6);
		quad.Corners[3].v = 64 << (c_FixedPointBits - 6);

		quad.Depth = VertexDepth(quad.Corners[0]);
		for (DWORD j = 1; j < 4; ++j) {
			long depth = VertexDepth(quad.Corners[j]);
			if (quad.Depth < depth) {
				quad.Depth = depth;
			}
		}

		if (quad.Corners[0].z < c_ZNear) {
			long frac = FixedDiv((c_ZNear - quad.Corners[0].z), (quad.Corners[1].z - quad.Corners[0].z));

			LerpVertex(quad.Corners[0], quad.Corners[1], quad.Corners[0], frac);
			LerpVertex(quad.Corners[3], quad.Corners[2], quad.Corners[3], frac);
		}
		else if (quad.Corners[1].z < c_ZNear) {
			long frac = FixedDiv((c_ZNear - quad.Corners[1].z), (quad.Corners[0].z - quad.Corners[1].z));

			LerpVertex(quad.Corners[1], quad.Corners[0], quad.Corners[1], frac);
			LerpVertex(quad.Corners[2], quad.Corners[3], quad.Corners[2], frac);
		}

		if (TransformVertices(quad.Corners, 4)) {
			quad.ShapeID  = shapeID;
			quad.Flags    = flags | 4;
			quad.pTexture = pTexture;

			++m_QuadCount;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SortQuadList()
//
void CwaWorld::SortQuadList(void)
{
	PROFILE(PFID_World_SortQuadList);

	m_QuickSortOverflow = false;

	for (DWORD quadNum = 0; quadNum < m_QuadCount; ++quadNum) {
		Quad_t &quad = m_QuadList[quadNum];

		m_QuadSortList[quadNum].Depth = quad.Depth;
		m_QuadSortList[quadNum].Index = quadNum;

		if (QuadFlag_ZBiasCloser & quad.Flags) {
			m_QuadSortList[quadNum].Depth -= (c_FixedPointOne >> 3);
		}
		else if (QuadFlag_ZBiasFarther & quad.Flags) {
			m_QuadSortList[quadNum].Depth += IntToFixed(3) >> 2;
		}
	}

	if (m_QuadCount > 1) {
		DWORD  stackSize  = 4096;
		DWORD  stackIndex = 0;
		DWORD* pStack     = (DWORD*)alloca(stackSize * sizeof(DWORD));
		long   pivotLeft  = 0;
		long   pivotRight = m_QuadCount - 1;

		for (;;) {
			// When the subarray is small enough, use insertion sort.
			if ((pivotRight - pivotLeft) < 7) {
				for (long tempRight = pivotLeft + 1; tempRight <= pivotRight; ++tempRight) {
					long tempDepth = m_QuadSortList[tempRight].Depth;
					long tempIndex = m_QuadSortList[tempRight].Index;
					for (long tempLeft = tempRight - 1; tempLeft >= long(pivotLeft); --tempLeft) {
						if (m_QuadSortList[tempLeft].Depth < tempDepth) {
							break;
						}

						m_QuadSortList[tempLeft + 1].Depth = m_QuadSortList[tempLeft].Depth;
						m_QuadSortList[tempLeft + 1].Index = m_QuadSortList[tempLeft].Index;
					}
					m_QuadSortList[tempLeft + 1].Depth = tempDepth;
					m_QuadSortList[tempLeft + 1].Index = tempIndex;
				}

				// Once the stack returns to being empty (or is initially empty
				// and the total count of items to sort is small), break out of
				// the main loop.  Things should all be sorted now.
				if (stackIndex == 0) {
					break;
				}

				// Pop the stack and begin a new round of partitioning.
				pivotRight = pStack[stackIndex--];
				pivotLeft  = pStack[stackIndex--];
			}
			else {

				// Derived from Lewis & Denenberg, pg 390.
				long tempLeft  = pivotLeft;
				long tempRight = pivotRight + 1;
				long tempDepth = m_QuadSortList[pivotLeft].Depth;
//				long tempIndex = m_QuadSortList[pivotLeft].Index;

				while (tempLeft < tempRight) {
					++tempLeft;
					while ((tempLeft <= pivotRight) && (m_QuadSortList[tempLeft].Depth < tempDepth)) {
						++tempLeft;
					}
					--tempRight;
					while ((tempRight >= pivotLeft) && (m_QuadSortList[tempRight].Depth > tempDepth)) {
						--tempRight;
					}
					if (tempLeft <= pivotRight) {
						Swap(m_QuadSortList[tempLeft].Depth, m_QuadSortList[tempRight].Depth);
						Swap(m_QuadSortList[tempLeft].Index, m_QuadSortList[tempRight].Index);
					}
				}
				if (tempLeft <= pivotRight) {
					Swap(m_QuadSortList[tempLeft].Depth, m_QuadSortList[tempRight].Depth);
					Swap(m_QuadSortList[tempLeft].Index, m_QuadSortList[tempRight].Index);
				}
				Swap(m_QuadSortList[tempRight].Depth, m_QuadSortList[pivotLeft].Depth);
				Swap(m_QuadSortList[tempRight].Index, m_QuadSortList[pivotLeft].Index);
				pStack[++stackIndex] = pivotLeft;
				pStack[++stackIndex] = tempRight - 1;
				pivotLeft = tempRight + 1;

				// Sanity check: Should the stack approach overflow, give up and bubble
				// sort it.  This only occurs when viewing large empty levels with no
				// walls or furnishings.  Floor quads end up providing a nearly sorted
				// list, inducing worst-case behavior from quicksort.
				if ((stackIndex + 2) >= stackSize) {
					for (DWORD i = 0; i < m_QuadCount; ++i) {
						DWORD swapCount = 0;

						for (DWORD j = i + 1; j < m_QuadCount; ++j) {
							if (m_QuadSortList[i].Depth > m_QuadSortList[j].Depth) {
								Swap(m_QuadSortList[i].Depth, m_QuadSortList[j].Depth);
								Swap(m_QuadSortList[i].Index, m_QuadSortList[j].Index);
								++swapCount;
							}
						}

						// Stop sorting when everything is sorted.
						if (0 == swapCount) {
							break;
						}
					}

					m_QuickSortOverflow = true;

					return;
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderQuadList()
//
void CwaWorld::RenderQuadList(void)
{
	PROFILE(PFID_World_RenderQuadList);

	long quadCount = m_QuadCount;

	if ((0 != m_QuadLimit) && (DWORD(quadCount) > m_QuadLimit)) {
		quadCount = m_QuadLimit;
	}

	for (long quadNum = quadCount - 1; quadNum >= 0; --quadNum) {
		Quad_t &quad = m_QuadList[m_QuadSortList[quadNum].Index];

		m_pRaster->SetTexture(quad.pTexture);

		if (QuadFlag_Ngon & quad.Flags) {
			if (quad.Corners[0].z > IntToFixed(6)) {
				m_pRaster->PaintNgonFast(quad.Corners, (quad.Flags & QuadFlag_CornerMask));
			}
			else {
				m_pRaster->PaintNgon(quad.Corners, (quad.Flags & QuadFlag_CornerMask));
			}
		}
		else if (QuadFlag_Translucent & quad.Flags) {
			m_pRaster->PaintSpriteTranslucent(quad.Corners);
		}
		else if (QuadFlag_Sprite & quad.Flags) {
			m_pRaster->PaintSprite(quad.Corners);
		}
		else if (QuadFlag_Transparent & quad.Flags) {
			m_pRaster->PaintVerticalQuadTransparent(quad.Corners);
		}
		else {
			m_pRaster->PaintVerticalQuad(quad.Corners);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	RenderQuadListWithDepth()
//
void CwaWorld::RenderQuadListWithDepth(void)
{
	PROFILE(PFID_World_RenderQuadListWithDepth);

	m_pRaster->ClearDepthBuffer();

	long quadCount = m_QuadCount;

	if ((0 != m_QuadLimit) && (DWORD(quadCount) > m_QuadLimit)) {
		quadCount = m_QuadLimit;
	}

	for (long quadNum = quadCount - 1; quadNum >= 0; --quadNum) {
		Quad_t &quad = m_QuadList[m_QuadSortList[quadNum].Index];

		m_pRaster->SetTexture(quad.pTexture);

		if (QuadFlag_Ngon & quad.Flags) {
		}
		else {
			m_pRaster->PaintVerticalQuadDepthID(quad.Corners, quad.ShapeID);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ClipQuadToNearZ()
//
bool CwaWorld::ClipQuadToNearZ(Quad_t &quad)
{
	PROFILE(PFID_World_ClipQuadToNearZ);

	DWORD clipper = 0;

#define zclip c_ZNear

	// Test each corner of the quad against the near-Z plane. Use these tests
	// to construct a bitmask that indicates which corners need to be clipped.

	if (quad.Corners[0].z < zclip) {
		clipper |= 1;
	}
	if (quad.Corners[1].z < zclip) {
		clipper |= 2;
	}
	if (quad.Corners[2].z < zclip) {
		clipper |= 4;
	}
	if (quad.Corners[3].z < zclip) {
		clipper |= 8;
	}

	DWORD cornerCount = 4;

	if (clipper) {
		if ((1 == clipper) || (2 == clipper) || (4 == clipper) || (8 == clipper)) {
			Vertex_t verts[4];

			if (1 == clipper) {
				verts[0] = quad.Corners[0];
				verts[1] = quad.Corners[1];
				verts[2] = quad.Corners[2];
				verts[3] = quad.Corners[3];
			}
			else if (2 == clipper) {
				verts[0] = quad.Corners[1];
				verts[1] = quad.Corners[2];
				verts[2] = quad.Corners[3];
				verts[3] = quad.Corners[0];
			}
			else if (4 == clipper) {
				verts[0] = quad.Corners[2];
				verts[1] = quad.Corners[3];
				verts[2] = quad.Corners[0];
				verts[3] = quad.Corners[1];
			}
			else {
				verts[0] = quad.Corners[3];
				verts[1] = quad.Corners[0];
				verts[2] = quad.Corners[1];
				verts[3] = quad.Corners[2];
			}

			long frac1 = FixedDiv((zclip - verts[0].z), (verts[3].z - verts[0].z));
			long frac2 = FixedDiv((zclip - verts[0].z), (verts[1].z - verts[0].z));

			LerpVertex(verts[0], verts[3], quad.Corners[0], frac1);
			LerpVertex(verts[0], verts[1], quad.Corners[1], frac2);

			quad.Corners[2] = verts[1];
			quad.Corners[3] = verts[2];
			quad.Corners[4] = verts[3];

			cornerCount = 5;
		}
		else if ((3 == clipper) || (6 == clipper) || (12 == clipper) || (9 == clipper)) {
			Vertex_t verts[4];

			if (3 == clipper) {
				verts[0] = quad.Corners[0];
				verts[1] = quad.Corners[1];
				verts[2] = quad.Corners[2];
				verts[3] = quad.Corners[3];
			}
			else if (6 == clipper) {
				verts[0] = quad.Corners[1];
				verts[1] = quad.Corners[2];
				verts[2] = quad.Corners[3];
				verts[3] = quad.Corners[0];
			}
			else if (12 == clipper) {
				verts[0] = quad.Corners[2];
				verts[1] = quad.Corners[3];
				verts[2] = quad.Corners[0];
				verts[3] = quad.Corners[1];
			}
			else {
				verts[0] = quad.Corners[3];
				verts[1] = quad.Corners[0];
				verts[2] = quad.Corners[1];
				verts[3] = quad.Corners[2];
			}

			long frac1 = FixedDiv((zclip - verts[0].z), (verts[3].z - verts[0].z));
			long frac2 = FixedDiv((zclip - verts[1].z), (verts[2].z - verts[1].z));

			LerpVertex(verts[0], verts[3], quad.Corners[0], frac1);
			LerpVertex(verts[1], verts[2], quad.Corners[1], frac2);

			quad.Corners[2] = verts[2];
			quad.Corners[3] = verts[3];
		}
		else if ((14 == clipper) || (13 == clipper) || (11 == clipper) || (7 == clipper)) {
			Vertex_t verts[3];

			if (14 == clipper) {
				verts[0] = quad.Corners[3];
				verts[1] = quad.Corners[0];
				verts[2] = quad.Corners[1];
			}
			else if (13 == clipper) {
				verts[0] = quad.Corners[0];
				verts[1] = quad.Corners[1];
				verts[2] = quad.Corners[2];
			}
			else if (11 == clipper) {
				verts[0] = quad.Corners[1];
				verts[1] = quad.Corners[2];
				verts[2] = quad.Corners[3];
			}
			else {
				verts[0] = quad.Corners[2];
				verts[1] = quad.Corners[3];
				verts[2] = quad.Corners[0];
			}

			long frac1 = FixedDiv((zclip - verts[0].z), (verts[1].z - verts[0].z));
			long frac2 = FixedDiv((zclip - verts[2].z), (verts[1].z - verts[2].z));

			LerpVertex(verts[0], verts[1], quad.Corners[0], frac1);
			LerpVertex(verts[2], verts[1], quad.Corners[2], frac2);

			quad.Corners[1] = verts[1];

			cornerCount = 3;
		}
		else {
			return false;
		}
	}

	quad.Flags = cornerCount;

	return (TransformVertices(quad.Corners, cornerCount));
}


/////////////////////////////////////////////////////////////////////////////
//
//	ClipNearZ()
//
//	Clips a list of triangles to the near Z plane.  If the triangle does
//	cross the Z axis, it can turn into 0, 1, or 2 triangles.
//
//	Note that at this stage, the coordinates are still in player-local
//	space -- everything has been translated so the player is at the
//	origin, and rotated so the player is facing along the Z axis.
//
void CwaWorld::ClipNearZ(void)
{
	SwapInputOutputTriangleQueues();

	Triangle_t *pInput;

	long frac1, frac2;
	Triangle_t *pTri1;
	Triangle_t *pTri2;

	// Loop over every triangle in the input buffer, moving each one to
	// the output buffer, or destroying it if not visible.
	while (NULL != (pInput = PopInputTriangle())) {
		DWORD clipper = 0;

		Vertex_t &c1 = pInput->Corners[0];
		Vertex_t &c2 = pInput->Corners[1];
		Vertex_t &c3 = pInput->Corners[2];

		// Test each corner of the triangle against the near-Z plane. Use
		// these tests to construct a bitmask that indicates which corners
		// need to be clipped.

		if (c1.z < c_ZNear) {
			clipper |= 1;
		}
		if (c2.z < c_ZNear) {
			clipper |= 2;
		}
		if (c3.z < c_ZNear) {
			clipper |= 4;
		}

		switch (clipper) {
			case 0:
				// No intersection.  Just push the triangle onto the output queue.
				PushOutputTriangle(pInput);
				break;

			case 1: // c1
				pTri1 = NewTriangle();
				pTri2 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c1.z), (c2.z - c1.z));
				frac2 = FixedDiv((c_ZNear - c1.z), (c3.z - c1.z));

				LerpVertex(c1, c2, pTri1->Corners[0], frac1);
				LerpVertex(c1, c3, pTri2->Corners[2], frac2);

				pTri1->Corners[1] = c2;
				pTri1->Corners[2] = c3;

				pTri2->Corners[0] = pTri1->Corners[0];
				pTri2->Corners[1] = c3;

				PushOutputTriangle(pTri1);
				PushOutputTriangle(pTri2);

				FreeTriangle(pInput);
				break;

			case 2: // c2
				pTri1 = NewTriangle();
				pTri2 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c2.z), (c1.z - c2.z));
				frac2 = FixedDiv((c_ZNear - c2.z), (c3.z - c2.z));

				LerpVertex(c2, c1, pTri1->Corners[1], frac1);
				LerpVertex(c2, c3, pTri1->Corners[2], frac2);

				pTri1->Corners[0] = c1;

				pTri2->Corners[0] = c1;
				pTri2->Corners[1] = pTri1->Corners[2];
				pTri2->Corners[2] = c3;

				PushOutputTriangle(pTri1);
				PushOutputTriangle(pTri2);

				FreeTriangle(pInput);
				break;

			case 3: // c1 and c2
				pTri1 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c1.z), (c3.z - c1.z));
				frac2 = FixedDiv((c_ZNear - c2.z), (c3.z - c2.z));

				LerpVertex(c1, c3, pTri1->Corners[0], frac1);
				LerpVertex(c2, c3, pTri1->Corners[1], frac2);

				pTri1->Corners[2] = c3;

				PushOutputTriangle(pTri1);

				FreeTriangle(pInput);
				break;

			case 4: // c3
				pTri1 = NewTriangle();
				pTri2 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c3.z), (c1.z - c3.z));
				frac2 = FixedDiv((c_ZNear - c3.z), (c2.z - c3.z));

				LerpVertex(c3, c1, pTri2->Corners[2], frac1);
				LerpVertex(c3, c2, pTri2->Corners[1], frac2);

				pTri1->Corners[0] = c1;
				pTri1->Corners[1] = c2;
				pTri1->Corners[2] = pTri2->Corners[1];

				pTri2->Corners[0] = c1;

				PushOutputTriangle(pTri1);
				PushOutputTriangle(pTri2);

				FreeTriangle(pInput);
				break;

			case 5: // c1 and c3
				pTri1 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c1.z), (c2.z - c1.z));
				frac2 = FixedDiv((c_ZNear - c3.z), (c2.z - c3.z));

				LerpVertex(c1, c2, pTri1->Corners[0], frac1);
				LerpVertex(c3, c2, pTri1->Corners[2], frac2);

				pTri1->Corners[1] = c2;

				PushOutputTriangle(pTri1);

				FreeTriangle(pInput);
				break;

			case 6: // c2 and c3
				pTri1 = NewTriangle();

				frac1 = FixedDiv((c_ZNear - c2.z), (c1.z - c2.z));
				frac2 = FixedDiv((c_ZNear - c3.z), (c1.z - c3.z));

				LerpVertex(c2, c1, pTri1->Corners[1], frac1);
				LerpVertex(c3, c1, pTri1->Corners[2], frac2);

				pTri1->Corners[0] = c1;

				PushOutputTriangle(pTri1);

				FreeTriangle(pInput);
				break;

			case 7: // c1, c2,  and c3
				// Do nothing, the triangle is fully behind the clip plane.
				FreeTriangle(pInput);
				break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	TransformVertices()
//
//	Transforms vertices from player-centeric coordinates to view coordinates.
//	Also does some simple bounds testing, returning false if all of the
//	vertices are off the same side of the screen.
//
//	Assuming the vertices are all part of a triangle or quad, don't render
//	the geometry if it returns false.  This will eliminate the overhead
//	of attempting to render non-visible geometry.
//
bool CwaWorld::TransformVertices(Vertex_t corners[], DWORD count)
{
	PROFILE(PFID_World_TransformVertices);

	DWORD mask = 0xF;

	const long right  = m_HalfWidth  << (1 + c_FixedPointBits);
	const long bottom = m_HalfHeight << (1 + c_FixedPointBits);

	for (DWORD v = 0; v < count; ++v) {
		Vertex_t &corner = corners[v];

		// Scale x/y coords to screen resolution, where (0,0) is the center
		// of the screen.
		corner.x = FixedMul(corner.x, m_FieldOfViewX);
		corner.y = FixedMul(corner.y, m_FieldOfViewY);

		long inverse = FixedDiv(c_FixedPointOne, corner.z);

		// Apply perspective division.
		corner.x = FixedMul(corner.x, inverse);
		corner.y = FixedMul(corner.y, inverse);

		// Translate x/y into pixel coordinates so (0,0) is the upper left
		// corner of the screen.
		corner.x = (corner.x * m_HalfWidth) + (m_HalfWidth << c_FixedPointBits);
		corner.y = (m_HalfHeight << c_FixedPointBits) - (corner.y * m_HalfHeight);

		if (corner.x > 0) {
			mask &= ~8;
		}
		if (corner.x < right) {
			mask &= ~4;
		}
		if (corner.y > 0) {
			mask &= ~2;
		}
		if (corner.y < bottom) {
			mask &= ~1;
		}
	}

	// If any of the bits are still set, then all of the vertices are off the
	// same side of the screen, and therefore definitely not visible.  If all
	// of the bits have been cleared, then the geometry is probably visible,
	// but there are still special case exceptions.
	//
	// Just filtering out most of the non-visible geometry is good enough.
	//
	return (mask == 0);
}


