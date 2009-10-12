/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Animation.cpp 9     12/31/06 10:45a Lee $
//
//	File: Animation.cpp
//
//
//	This file contains several utility classes for storing textures and
//	animation sequences (each animation being composed of several individual
//	textures).  All of the internal logic required to load and decompress
//	image data is hidden under here (most of it resides in ParseBSA.cpp).
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"
#include "Animation.h"
#include "ParseBSA.h"
#include "TGA.h"


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaTexture::CwaTexture(void)
	:	m_Width(0),
		m_Height(0),
		m_OffsetX(0),
		m_OffsetY(0),
		m_Pitch(0),
		m_pTexels(NULL),
		m_pPalette(NULL)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaTexture::~CwaTexture(void)
{
	Free();
}


/////////////////////////////////////////////////////////////////////////////
//
//	Allocate()
//
//	Allocates a new texture buffer.  This defaults to making the image have
//	one extra pixel along the right and bottom edges, so that special cases
//	aren't needed to clamp or wrap textures properly.
//
void CwaTexture::Allocate(DWORD width, DWORD height, bool expand)
{
	Free();

	m_Width  = width;
	m_Height = height;
	m_Pitch  = width;

	if (expand) {
		m_Pitch += 1;
	}

	m_pTexels = new BYTE[m_Pitch * (m_Height + (expand ? 1 : 0))];
}


/////////////////////////////////////////////////////////////////////////////
//
//	Free()
//
void CwaTexture::Free(void)
{
	m_Width  = 0;
	m_Height = 0;
	m_Pitch  = 0;

	SafeDeleteArray(m_pTexels);
	SafeDeleteArray(m_pPalette);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadIMG()
//
void CwaTexture::LoadIMG(char filename[], bool clamp)
{
	PROFILE(PFID_Load_IMG);

	Free();

	DWORD width, height;
	bool  hasHeader;

	g_pArenaLoader->CheckIMG(filename, width, height, m_OffsetX, m_OffsetY, hasHeader);

	Allocate(width, height);

	m_pPalette = g_pArenaLoader->LoadIMG(filename, width, height, m_Pitch, hasHeader, m_pTexels);

	Clamp(clamp);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadWallTexture()
//
void CwaTexture::LoadWallTexture(char filename[], DWORD index, bool clamp)
{
	PROFILE(PFID_Load_WallTexture);

	Free();

	DWORD resID = g_pArenaLoader->LookUpResource(filename);

	if (DWORD(-1) != resID) {
		Allocate(64, 64, false);

		g_pArenaLoader->LoadWallTexture(resID, m_pTexels, (index * 64 * 64));
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	Clamp()
//
//	Assuming that the texture was allocated with extra pixels, this will set
//	up the texture to properly wrap or clamp for sampling purposes.
//
//	This removes the need for special cases when rendering.  If sampling a
//	pixel near the end of the line (or the bottom of the texture), sampling
//	logic needs extra <if> conditions to handle different special cases,
//	which slows rendering time.  Presetting the texture like this will
//	eliminate the need for that extra logic, making rendering faster.
//
void CwaTexture::Clamp(bool clamp)
{
	// First loop over each line of the frame, adding an extra pixel at the
	// end of the line.
	for (DWORD y = 0; y < m_Height; ++y) {
		BYTE *pLine = m_pTexels + (y * m_Pitch);

		// Clamp by replicating the last pixel on the line.
		if (clamp) {
			pLine[m_Width] = pLine[m_Width-1];
		}

		// Wrap by copying the first pixel to the end of the line.
		else {
			pLine[m_Width] = pLine[0];
		}
	}

	// Then do something similar by adding an extra line below the texture,
	// either duplicating the first line of the image (to wrap) or the last
	// valid line (to clamp).

	BYTE *pSrc = clamp ? (m_pTexels + ((m_Height-1) * m_Pitch)) : m_pTexels;
	BYTE *pDst = m_pTexels + (m_Height * m_Pitch);

	for (DWORD x = 0; x <= m_Width; ++x) {
		pDst[x] = pSrc[x];
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SaveTexture()
//
//	Takes whatever the current texture is (as set by SetTexture(), or the
//	default checkerboard texture if no texture has been provided), and writes
//	it off to a 32-bit TGA file.
//
//	Compression would be nice, to reduce file size.  Plus most muggles won't
//	have a clue how to view a TGA file, so making it a GIF would be useful.
//	But that requires adding libraries to the project, which I don't want to
//	do -- keeping binaries out of the project eliminates the possibility that
//	someone will embed trojans or other malware in those binaries.
//
void CwaTexture::SaveTexture(char filename[])
{
	TGAFileHeader header;
	memset(&header, 0, sizeof(header));
	header.ImageType                 = TGAImageType_ColorMapped;
	header.ColorMapType              = 1;
	header.ColorMapSpec.Length       = 256;
	header.ColorMapSpec.EntrySize    = 24;
	header.ImageSpec.ImageDescriptor = TGATopToBottom;
	header.ImageSpec.ImageWidth      = WORD(m_Width);
	header.ImageSpec.ImageHeight     = WORD(m_Height);
	header.ImageSpec.PixelDepth      = 8;

	TGAFooter footer;
	footer.devDirOffset  = 0;
	footer.extAreaOffset = 0;
	strcpy(footer.signature, TGASignature);

	BYTE palette[3*256];
	g_pArenaLoader->GetPaletteBlueFirst(Palette_Default, palette);

	FILE *pFile = fopen(filename, "wb");
	if (NULL != pFile) {
		fwrite(&header, sizeof(header), 1, pFile);
		fwrite(palette, 1, 3 * 256, pFile);
		for (DWORD y = 0; y < m_Height; ++y) {
			fwrite(m_pTexels + (y * m_Pitch), 1, m_Width, pFile);
		}
		fwrite(&footer, sizeof(footer), 1, pFile);

		fclose(pFile);
	}
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaAnimation::CwaAnimation(void)
	:	m_Width(0),
		m_Height(0),
		m_FrameCount(0),
		m_pFrames(NULL)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaAnimation::~CwaAnimation(void)
{
	SafeDeleteArray(m_pFrames);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadAnimation()
//
//	Loads animation data from a CFA or DFA file.  These animations are used
//	in similar ways, but are encoded differenly, so different code paths are
//	needed to extract the animation data into a useable format.
//
bool CwaAnimation::LoadAnimation(char filename[])
{
	PROFILE(PFID_Load_Animation);

	SafeDeleteArray(m_pFrames);

	m_Width      = 0;
	m_Height     = 0;
	m_FrameCount = 0;

	DWORD resID = g_pArenaLoader->LookUpResource(filename);

	if (DWORD(-1) == resID) {
		return false;
	}

	char *extension = strstr(filename, ".");
	if (0 == strcmp(extension, ".CFA")) {
		g_pArenaLoader->CheckCFA(resID, m_Width, m_Height, m_FrameCount);

		if ((m_Width == 0) || (m_Height == 0) || (m_FrameCount == 0)) {
			return false;
		}

		m_pFrames = new CwaTexture[m_FrameCount];

		BYTE **ppFrames = reinterpret_cast<BYTE**>(alloca(m_FrameCount * sizeof(BYTE*)));

		for (DWORD frameNum = 0; frameNum < m_FrameCount; ++frameNum) {
			m_pFrames[frameNum].Allocate(m_Width, m_Height);

			ppFrames[frameNum] = m_pFrames[frameNum].m_pTexels;
		}

		g_pArenaLoader->LoadCFA(resID, m_Width, m_Height, m_Width + 1, m_FrameCount, ppFrames);

		for (DWORD frameNum = 0; frameNum < m_FrameCount; ++frameNum) {
			m_pFrames[frameNum].Clamp(true);
		}
	}
	else if (0 == strcmp(extension, ".DFA")) {
		g_pArenaLoader->CheckDFA(resID, m_Width, m_Height, m_FrameCount);

		if ((m_Width == 0) || (m_Height == 0) || (m_FrameCount == 0)) {
			return false;
		}

		m_pFrames = new CwaTexture[m_FrameCount];

		for (DWORD frameNum = 0; frameNum < m_FrameCount; ++frameNum) {
			m_pFrames[frameNum].Allocate(m_Width, m_Height);

			g_pArenaLoader->LoadDFA(resID, m_Width, m_Height, m_Width + 1, frameNum, m_pFrames[frameNum].m_pTexels);

			m_pFrames[frameNum].Clamp(true);
		}
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadCIF()
//
//	Loads a CIF (Compressed Image File?) image into the animation.  This
//	will contain a sequence of frames.  Most CIFs fall into one of three
//	categories: character faces, equipment icons, and weapon animations.
//
bool CwaAnimation::LoadCIF(char filename[], DWORD frameCount)
{
	PROFILE(PFID_Load_CIF);

	SafeDeleteArray(m_pFrames);

	m_FrameCount = frameCount;

	m_pFrames = new CwaTexture[m_FrameCount];

	for (DWORD frameNum = 0; frameNum < frameCount; ++frameNum) {
		DWORD width, height, offsetX, offsetY;

		g_pArenaLoader->CheckCIF(filename, frameNum, width, height, offsetX, offsetY);

		m_pFrames[frameNum].Allocate(width, height);

		// Record the offsets.  These are essential for weapon animations.
		m_pFrames[frameNum].m_OffsetX = offsetX;
		m_pFrames[frameNum].m_OffsetY = offsetY;

		if (0 == frameNum) {
			m_Width  = width;
			m_Height = height;
		}

		g_pArenaLoader->LoadCIF(filename, width, height, frameNum, m_pFrames[frameNum].m_pTexels);
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadWater()
//
//	The water and lava animations are identical, both in how they are stored
//	and rendered.  They are found in RCI files, contain 5 frames of 320x100
//	image data, uncompressed, without any header information.
//
void CwaAnimation::LoadWater(bool lava)
{
	PROFILE(PFID_Load_Water);

	SafeDeleteArray(m_pFrames);
	
	m_Width      = 320;
	m_Height     = 100;
	m_FrameCount = 5;

	m_pFrames = new CwaTexture[m_FrameCount];

	for (DWORD frameNum = 0; frameNum < m_FrameCount; ++frameNum) {
		m_pFrames[frameNum].Allocate(m_Width-1, m_Height);

		g_pArenaLoader->LoadWater(lava ? "LAVAANI.RCI" : "WATERANI.RCI", frameNum, m_pFrames[frameNum].m_pTexels);
	}
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaCreature::CwaCreature(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaCreature::~CwaCreature(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadAnimation()
//
//	Given the base name of a creature animation, all 6 animation sequences
//	will be loaded.  For example, a "RAT" animation is stored in the files,
//	"RAT1.CFA", "RAT2.CFA", ... "RAT6.CFA."
//
//	Each animation sequence shows the creature from a different point of view.
//	RAT1.CFA shows a rat facing the player (and also includes an attack
//	animation, unlike the other sequences), while RAT6.CFA shows the rat
//	facing directly away from the player.
//
void CwaCreature::LoadAnimation(char basename[])
{
	char filename[64];

	for (DWORD i = 0; i < 6; ++i) {
		sprintf(filename, "%s%d.CFA", basename, i + 1);
		m_Animation[i].LoadAnimation(filename);
	}
}


