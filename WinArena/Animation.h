/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Animation.h 4     12/31/06 10:45a Lee $
//
//	File: Animation.h
//
//
//	This file contains several utility classes for storing textures and
//	animation sequences (each animation being composed of several individual
//	textures).  All of the internal logic required to load and decompress
//	image data is hidden under here (most of it resides in ParseBSA.cpp).
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


/////////////////////////////////////////////////////////////////////////////
//
//	CwaTexture
//
//	NOTE: All textures are allocated with one extra row and column of pixels,
//	which duplicate either the first or last row/column.  This eliminates the
//	need to deal with some textures being wrapped and others clamped.  Wrapped
//	textures duplicate the first row/column, while clamped textures duplicate
//	the last row/column.  So we only need to care about this when the texture
//	is loaded.
//
class CwaTexture
{
public:
	DWORD  m_Width;
	DWORD  m_Height;
	DWORD  m_OffsetX;		// offsets are used to position some animations (like weapons) on the screen
	DWORD  m_OffsetY;
	DWORD  m_Pitch;			// usually set to m_Width + 1
	BYTE*  m_pTexels;		// usually ((m_Width + 1) * (m_Height + 1)) pixels
	DWORD* m_pPalette;		// any custom palette required is stored here, dynamically allocated

	CwaTexture(void);
	~CwaTexture(void);

	void Allocate(DWORD width, DWORD height, bool expand = true);
	void Free(void);
	void LoadIMG(char filename[], bool clamp);
	void LoadWallTexture(char filename[], DWORD index, bool clamp);
	void Clamp(bool clamp);
	void SaveTexture(char filename[]);

	BYTE GetPixel(DWORD x, DWORD y)
	{
		return m_pTexels[(m_Pitch * y) + x];
	}

	// Exactly 64x64, with pitch == width.
	BYTE GetWallPixel(DWORD x, DWORD y)
	{
		return m_pTexels[(y << 6) + x];
	}
};


/////////////////////////////////////////////////////////////////////////////
//
//	CwaAnimation
//
//	Stores a sequence of textures forming an animation.
//
//	NOTE: Not all animations keep the same size for every frame.  Monster
//	and weapon animations are most likely to vary in size from frame to frame,
//	which makes rendering them cumbersome.  Some of the animations have X & Y
//	offsets that indicate the respective positioning for each frame.  This is
//	usually meaningful for weapon animations or animations drawn directly on
//	top of the screen (things that are drawn 1:1 in pixel pitch), but things
//	drawn off in the distance seem off, or don't have offset values.  More
//	experimentation is needed to figure out how reliable these are.
//
class CwaAnimation
{
public:
	DWORD		m_Width;
	DWORD		m_Height;
	DWORD		m_FrameCount;
	CwaTexture*	m_pFrames;


	CwaAnimation(void);
	~CwaAnimation(void);

	bool LoadAnimation(char filename[]);
	bool LoadCIF(char filename[], DWORD frameCount);
	void LoadWater(bool lava = false);
};


/////////////////////////////////////////////////////////////////////////////
//
//	CwaCreature
//
//	The animation data for creatures contain 6 orientations, from facing
//	towards the player to facing away.  Mirror orientations 2-5 to allow the
//	creature to turn a complete circle.  This has the side effect of swapping
//	the weapon and shield hands for the creature, but most 2D spite-based
//	games do this trick.
//
//	Not all creatures contain the same sequence of animations.  Monsters with
//	weapons usually have both an idle animation and an attack animation (such
//	as goblins), while animals may not have an idle animation (homonculi, but
//	they fly, so stopping the wing-flapping animation doesn't make sense).
//
//	For most monsters, the first few frames are a walking animation, followed
//	by an idle/looking-around animation, and finally an attack animation
//	(but only when facing towards the player).  In the case of humans, there
//	are separate equipment animations, so shields and weapons need to added
//	on top of some character animations.
//
//	NOTE: A common problem with monster animations is that they vary in size
//	across different orientations, and the monsters are not always centered
//	in the middle of the frame (especially if holding a weapon in one hand),
//	so additional tweaking is needed to scale each animation and keep it
//	centered so the monsters don't jump around or suddenly get bigger when
//	they change direction.
//
class CwaCreature
{
public:
	CwaAnimation	m_Animation[6];


	CwaCreature(void);
	~CwaCreature(void);

	void LoadAnimation(char basename[]);
};

