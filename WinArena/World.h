/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/World.h 10    12/31/06 12:11p Lee $
//
//	File: World.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#include "Animation.h"
#include "Raster.h"
#include "SoundDriver.h"
#include "Types.h"
#include "MidiPlayer.h"


#define c_PortalOpenTime	1400		// milliseconds required for door/portcullis to open


extern AnimInfo_t		g_AnimInfo[];
extern CritterInfo_t	g_CritterInfo[];	// g_CritterInfo[c_CritterTypeCount];
extern SoundFile_t		g_SoundFile[];
extern SongFile_t		g_SongFile[];


struct BlockSide_t
{
	DWORD Flags;
	long  yLo;
	long  yHi;
	long  vLo;
	long  vHi;
};


class CwaWorld
{
private:
	ArenaLoader		m_Loader;

	CwaSoundDriver	m_SoundDriver;

	CwaRaster*		m_pRaster;

	CwaAnimation*	m_pWeapon;

	bool			m_UseFogMap;
	BYTE			m_LightMapNormal[18*256];
	BYTE			m_LightMapFog[18*256];

	CwaTexture*		m_pCurrentMousePointer;
	bool			m_MouseMoveEnabled;
	long			m_MouseTurnRate;
	long			m_MouseTurnRateFraction;
	long			m_MouseWalkRate;
	long			m_MouseStrafeRate;

	SoundClip_t		m_SoundClips[Sound_ArraySize];
	CwaTexture		m_TextureList[Texture_ArraySize];
	CwaTexture		m_WallList[256];
	CwaTexture		m_FlatList[64];
	CwaAnimation	m_Animations[Animation_ArraySize];
	CwaCreature		m_Creatures[128];
	CwaAnimation	m_Water;
	CwaAnimation	m_WeaponAxe;
	CwaAnimation	m_WeaponHammer;
	CwaAnimation	m_WeaponHand;
	CwaAnimation	m_WeaponMace;
	CwaAnimation	m_WeaponStaff;
	CwaAnimation	m_WeaponStar;
	CwaAnimation	m_WeaponSword;
	CwaAnimation	m_MousePointer;

	CwaAnimation	m_SpellAcid;
	CwaAnimation	m_SpellFire;
	CwaAnimation	m_SpellIce;
	CwaAnimation	m_SpellMagic;
	CwaAnimation	m_SpellShock;
	CwaAnimation	m_SpellSmoke;

	CastingSpell_t	m_SpellAnimation;
	DWORD			m_SpellCastBaseTime;

	AttackState_t	m_AttackState;
	DWORD			m_AttackStartTime;
	bool			m_AttackAnimating;

	CellNode_t		m_Nodes[128][128];

	Critter_t		m_Critter[128];
	Sprite_t		m_Sprite[1024];
	Projectile_t	m_Projectile[16];
	LightSource_t	m_Lights[128];
	Trigger_t		m_Triggers[64];

	DWORD			m_LightCount;

	CwaMidiPlayer	m_MidiPlayer;
	CwaMidiSong		m_SongList[Song_ArraySize];
	MidiEvent_t		m_MidiEvent;


	DWORD			m_TriggerCount;

	Character_t		m_Character;

	long			m_Position[3];
	long			m_Direction;
	long			m_DirectionSine;
	long			m_DirectionCosine;
	long			m_TurnSpeed;
	long			m_MoveSpeed;
	long			m_StrafeSpeed;
	DWORD			m_ElevationBaseTime;
	Elevation_t		m_Elevation;
	long			m_ElevationBeginPos[3];	// where player will land when falling
	long			m_ElevationEndPos[3];	// where player will land when falling
	DWORD			m_MaxEast;
	DWORD			m_MaxSouth;
	DWORD			m_MaxDrawDistance;		// only draw cells within this grid distance
	DWORD			m_WalkDistance;			// distance walked since last footstep sound
	bool			m_LeftFootStep;			// was most recent footstep sound left or right?
	long			m_HalfWidth;
	long			m_HalfHeight;
	long			m_FieldOfViewX;
	long			m_FieldOfViewY;
	long			m_InverseFieldOfViewX;
	long			m_InverseFieldOfViewY;
	bool			m_ShowFPS;
	bool			m_IlluminatePlayer;
	DWORD			m_FPS;
	DWORD			m_MouseX;
	DWORD			m_MouseY;
	bool			m_RightMouseDown;
	DWORD			m_RightMouseX;
	DWORD			m_RightMouseY;
	DWORD			m_BaseAttackTime;
	DWORD			m_RandomSoundDelay;
	DWORD			m_CurrentTimeDelta;
	DWORD			m_CurrentTime;
	DisplayMode_t	m_DisplayMode;
	bool			m_FullBright;			// render everything at full brightness, enabled by "Light" spell
	DWORD			m_SkyColor;				// sky/darkness color to use when outside
	PauseState_t	m_PauseState;			// game is paused, usually waiting for user input

	bool			m_SelectionCompleted;	// flag gets set to true when user has picked item from list
	DWORD			m_SelectionCount;		// number of items in selection list
	DWORD			m_SelectionIndex;		// index of currently highlighted item
	DWORD			m_SelectionMaxVisi;		// maximum number of items visible at once
	DWORD			m_SelectionOffset;		// index of first visible item
	DWORD			m_SelectionID;			// sprite ID when clicking on a loot pile or corpse
	char*			m_SelectionList[32];

	DWORD			m_LevelDownID;			// texture IDs for stairs up/down tiles
	DWORD			m_LevelUpID;

	Triangle_t		m_TriangleList[64];
	Triangle_t*		m_pInputTriangleQueue;
	Triangle_t*		m_pOutputTriangleQueue;
	Triangle_t*		m_pFreeTriangleQueue;

	DWORD			m_Palette[256];

	char			m_MessageBuffer[256];
	DWORD			m_MessageBaseTime;
	DWORD			m_MessageWidth;

	bool			m_QuickSortOverflow;
	DWORD			m_QuadLimit;			// maximum number of quads to render to improve performance, or 0 for no limit
	DWORD			m_QuadCount;
	QuadSort_t		m_QuadSortList[10000];	// sorting table, sorts quad indices instead of quads, so swapping is faster
	Quad_t			m_QuadList[10000];		// list of visible quads

	inline void LerpVertex(const Vertex_t &v1, const Vertex_t &v2, Vertex_t &result, long mul)
	{
		result.x = FixedMul((v2.x - v1.x), mul) + v1.x;
		result.y = FixedMul((v2.y - v1.y), mul) + v1.y;
		result.z = FixedMul((v2.z - v1.z), mul) + v1.z;
		result.u = FixedMul((v2.u - v1.u), mul) + v1.u;
		result.v = FixedMul((v2.v - v1.v), mul) + v1.v;
		result.l = FixedMul((v2.l - v1.l), mul) + v1.l;
	}

public:
	CwaWorld(void);
	~CwaWorld(void);

	bool  Load(void);
	void  Unload(void);

	void  ResampleAudio(BYTE input[], short output[], DWORD countIn, DWORD countOut, DWORD filterSize);

	void  DumpMusic();

	void  ShowFPS(bool enable);
	void  EnablePlayerIllumination(bool enable);
	void  SetFPS(DWORD fps);
	void  SetResolution(DWORD width, DWORD height);
	void  SetRaster(CwaRaster *pRaster);
	void  SetRenderDistance(DWORD distance);

	void  ScrollWheel(bool down);
	void  ScrollSelectMenuUp(void);
	void  ScrollSelectMenuDown(void);

	void  SetMousePosition(DWORD x, DWORD y);

	bool  LeftMouseClick(void);
	void  LeftMouseClickLootPile(DWORD x, DWORD y);

	void  RightMouseClick(bool down);

	void  AdvanceTime(DWORD delta);
	void  PressEscKey(void);

	DWORD Random32(DWORD maxValue);

	bool  CheckCritterCollision(long x, long z, DWORD &id);
	bool  CheckHitCritterMelee(DWORD &id);

	bool  ShowOutsideOfBlock(DWORD south, DWORD east, DWORD level, BlockSide_t &side);
	bool  IsBlockSolid(DWORD south, DWORD east, DWORD level);
	bool  CellIsVisible(DWORD south, DWORD east);
	DWORD CellTrigger(DWORD south, DWORD east);

	void  SetPlayerDirection(long direction);
	void  MovePlayer(DWORD milliseconds);
	void  FallIntoTrench(long southFrac, long eastFrac, long south, long east);
	void  Jump(void);
	void  SetTurnSpeed(long speed);
	void  SetMoveSpeed(long speed);
	void  CheckMouseMovement(void);
	void  PushPlayerBack(long east, long south, long minDistance);

	void  ClickOnID(DWORD pointID);
	void  SetDisplayMessage(char *pMessage = NULL);

	void  ResetTriangleQueues(void);
	void  SwapInputOutputTriangleQueues(void);

	Triangle_t* NewTriangle(void);
	void        FreeTriangle(Triangle_t *pTriangle);
	Triangle_t* PopInputTriangle(void);
	void        PushOutputTriangle(Triangle_t *pTriangle);
	Triangle_t* PopOutputTriangle(void);

	long  ComputeBrightness(long position[]);

	void  UpdateLightSources(void);
	void  UpdateMouseMoving(DWORD scale);
	void  UpdateTriggers(void);

	void  ShowArmorRatings(DWORD scale);

	void  ComputeGridCoords(GridCoords_t *pGrid, long lineNum);

	void  Render(void);
	void  RenderAutomap(DWORD scale);
	void  RenderCharacter(DWORD scale);
	void  RenderControlPanel(DWORD scale);
	void  RenderGameScreen(DWORD scale);
	void  RenderInventory(DWORD scale);
	void  RenderJournal(DWORD scale);
	void  RenderLoadSave(DWORD scale);
	void  RenderSpellBook(DWORD scale);
	void  RenderSpellPage(DWORD scale);
	void  RenderSpellCasting(DWORD scale);
	void  RenderWater(DWORD scale);
	void  RenderWeaponOverlay(DWORD scale);

	void  RenderCell(GridCoords_t *pGrid0, GridCoords_t *pGrid1, DWORD south, DWORD east);
	void  RenderTriQuad(Vertex_t corners[], DWORD shapeID, bool transparent = false);
	void  RenderSprite(DWORD spriteNum);
	void  RenderProjectile(DWORD projectileNum);
	void  RenderCritter(DWORD critterNum);
	void  RenderTriangleList(bool transparent, DWORD shapeID);

	bool  CreateHorizontalQuad(Quad_t &quad);
	void  AddHorizontalQuad(Quad_t &quad, CwaTexture *pTexture, DWORD shapeID = 0, DWORD flags = 0);
	void  AddWallQuad(Quad_t &quad, CwaTexture *pTexture, DWORD shapeID = 0, DWORD flags = 0);
	void  AddVerticalQuad(Vertex_t corners[], CwaTexture *pTexture, DWORD shapeID = 0, DWORD flags = 0);
	void  SortQuadList(void);
	void  RenderQuadList(void);
	void  RenderQuadListWithDepth(void);

	bool  ClipQuadToNearZ(Quad_t &quad);
	void  ClipNearZ(void);

	void  TransformPoint(long point[]);
	bool  TransformVertices(Vertex_t corners[], DWORD count);


	/////////////////////////////////////////////////////////////////////////
	//
	//	The following methods are implemented in WorldBuilder.cpp
	//

	void  ResetLevel(void);
	void  SetCellTrigger(DWORD south, DWORD east, DWORD type);
	long  PortalAngle(Trigger_t &trigger);
	DWORD AllocCritter(void);
	DWORD AllocLight(void);
	DWORD AllocSprite(void);
	void  FreeSprite(DWORD spriteID);
	DWORD AddAnimatedSprite(DWORD anim, long position[3]);
	DWORD MakeCritter(DWORD critterID, long position[3]);
	DWORD MakeFlatSprite(DWORD textureID, long position[3], DWORD scale);
	DWORD MakeLootPile(DWORD textureID, long position[3]);
	void  BuildTestLevel(void);
	void  BuildTestCity(void);
	void  BuildDemoDungeon(char datafile[], char info[], DWORD width, DWORD height, bool exterior);
	void  ParseInfoFlags(char *p, FlatInfo_t &flat);
	void  ParseINF(char name[], FlatInfo_t flats[]);
};


