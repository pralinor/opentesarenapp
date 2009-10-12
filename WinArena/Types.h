/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Types.h 4     12/31/06 11:09a Lee $
//
//	File: Types.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once



enum DisplayMode_t
{
	Display_Automap,
	Display_Character,
	Display_ControlPanel,
	Display_Game,
	Display_Inventory,
	Display_Journal,
	Display_LoadSave,
	Display_SpellBook,
	Display_SpellPage,
};


enum PauseState_t
{
	Pause_NotPaused,
	Pause_SelectSpell,
	Pause_SelectItem,
	Pause_SelectLoot,
};


enum AttackState_t
{
	Attack_Off,
	Attack_Idle,			// base 30
	Attack_Thrust,			// base 25
	Attack_Chop,			// base  0
	Attack_DownToRight,		// base 20
	Attack_LeftToRight,		// base 15
	Attack_DownToLeft,		// base  5
	Attack_RightToLeft		// base 10
};


enum CastingSpell_t
{
	Casting_Nothing,
	Casting_Acid,
	Casting_Fire,
	Casting_Ice,
	Casting_Magic,
	Casting_Shock,
	Casting_Smoke
};


enum TextureID_t
{
	Texture_NULL,

	Texture_TrenchCeiling,
	Texture_Abyss,

	Texture_ChestClosed,
	Texture_ChestOpen,
	Texture_Equipment,
	Texture_Treasure1,
	Texture_Treasure2,
	Texture_Treasure3,

	Texture_HUD,
	Texture_Pointer,
	Texture_CompassBorder,
	Texture_CompassLines,
	Texture_Automap,
	Texture_EquipmentPopup,
	Texture_ControlPanel,
	Texture_Journal,
	Texture_LoadSave,
	Texture_SpellBook,
	Texture_SpellPage,
	Texture_ArraySize
};



enum AnimationName_t
{
	Animation_Bartender,
	Animation_Begger,
	Animation_Brazier,
	Animation_Candle,
	Animation_ChaosArrow,
	Animation_ChaosPillar,
	Animation_ChaosStone,
	Animation_CrystalBall,
	Animation_CrystalStaff,
	Animation_DarkAltar,
	Animation_Dragonling,
	Animation_Fireplace,
	Animation_FireGem,
	Animation_FireBreather,
	Animation_FirePitCauldron,
	Animation_FirePitCook,
	Animation_Fishbowl,
	Animation_Guard1,
	Animation_Guard2,
	Animation_Guard3,
	Animation_Guard4,
	Animation_Guard5,
	Animation_GuardDeath,
	Animation_GuardSingle1,
	Animation_GuardSingle2,
	Animation_GuardPair1,
	Animation_GuardPair2,
	Animation_GuardTalk1,
	Animation_GuardTalk2,
	Animation_GuardRender,
	Animation_Hoker1,
	Animation_Hoker2,
	Animation_Hoker3,
	Animation_HokerPushups,
	Animation_Jester,
	Animation_ManFlaggon,
	Animation_ManNodding,
	Animation_ManTalking,
	Animation_ManFarmer,
	Animation_MerchantTable,
	Animation_MerchantBooth,
	Animation_Moon1,
	Animation_Moon2,
	Animation_Musician1,
	Animation_Musician2,
	Animation_StoneLamp,
	Animation_FountainDragon,
	Animation_FountainNormal,
	Animation_FountainLion,
	Animation_Streetlight,
	Animation_StreetlightSnowy,
	Animation_Priest1,
	Animation_Priest2,
	Animation_Puddle,
	Animation_Rat,
	Animation_ShadyThug,
	Animation_SkullCandle,
	Animation_Smith,
	Animation_SnakeCharmer,
	Animation_TorchPost,
	Animation_Vendor,
	Animation_VolcanoSmall,
	Animation_VolcanoDagothUr,
	Animation_VolcanoLarge,
	Animation_WaterColumn,
	Animation_WaterColumnBroken,
	Animation_Wizard,
	Animation_WomanLeather,
	Animation_WomanTankard,
	Animation_WomanHaughty,
	Animation_WomanBikini,
	Animation_Seaweed,
	Animation_WaterStatue1,
	Animation_WaterStatue2,

	Animation_BloodSpurt,
	Animation_AcidBurstLarge,
	Animation_AcidBurstSmall,
	Animation_AcidBulletLarge,
	Animation_AcidBulletSmall,
	Animation_FireBurstLarge,
	Animation_FireBurstSmall,
	Animation_FireBulletLarge,
	Animation_FireBulletSmall,
	Animation_GasBurstLarge,
	Animation_GasBurstSmall,
	Animation_GasBulletLarge,
	Animation_GasBulletSmall,
	Animation_IceBurstLarge,
	Animation_IceBurstSmall,
	Animation_IceBulletLarge,
	Animation_IceBulletSmall,
	Animation_MagicBurstLarge,
	Animation_MagicBurstSmall,
	Animation_MagicBulletLarge,
	Animation_MagicBulletSmall,
	Animation_ShockBurstLarge,
	Animation_ShockBurstSmall,
	Animation_ShockBulletLarge,
	Animation_ShockBulletSmall,
	Animation_ArraySize
};


enum SoundID_t
{
	Sound_ArrowFire,
	Sound_ArrowHit,
	Sound_Background,
	Sound_Bash,
	Sound_Birds1,
	Sound_Birds2,
	Sound_BodyFall,
	Sound_Burst,
	Sound_Clank,
	Sound_Clicks,
	Sound_CloseDoor,
	Sound_DeepChoir,
	Sound_DirtLeft,
	Sound_DirtRight,
	Sound_Drip1,
	Sound_Drip2,
	Sound_Drums,
	Sound_Eerie,
	Sound_EHit,
	Sound_Explode,
	Sound_Fanfare1,
	Sound_Fanfare2,
	Sound_FemaleDie,
	Sound_FireDaemon,
	Sound_Ghost,
	Sound_Ghoul,
	Sound_Grind,
	Sound_Growl0,
	Sound_Growl1,
	Sound_Growl2,
	Sound_Halt,
	Sound_HellHound,
	Sound_HighChoir,
	Sound_Homonculous,
	Sound_HumEerie,
	Sound_IceGolem,
	Sound_IronGolem,
	Sound_Lich,
	Sound_Lizard,
	Sound_Lock,
	Sound_MaleDie,
	Sound_Medusa,
	Sound_Minotaur,
	Sound_Monster,
	Sound_Moon,
	Sound_MudStepLeft,
	Sound_MudStepRight,
	Sound_NHit,
	Sound_OpenAlt,
	Sound_OpenDoor,
	Sound_Orc,
	Sound_Portcullis,
	Sound_Rat,
	Sound_Scream1,
	Sound_Scream2,
	Sound_Skeleton,
	Sound_SlowBall,
	Sound_Snarl,
	Sound_SnowLeft,
	Sound_SnowRight,
	Sound_Splash,
	Sound_Squish,
	Sound_StoneGolem,
	Sound_StopThief,
	Sound_Swimming,
	Sound_Swish,
	Sound_Thunder,
	Sound_Troll,
	Sound_UHit,
	Sound_Umph,
	Sound_Vampire,
	Sound_Whine,
	Sound_Wind,
	Sound_Wolf,
	Sound_Wraith,
	Sound_Zombie,
	Sound_ArraySize
};


enum SongID_t
{
	Song_ArabVillage,
	Song_ArabCity,
	Song_ArabTown,
	Song_City,
	Song_Combat,
	Song_Credits,
	Song_Dungeon1,
	Song_Dungeon2,
	Song_Dungeon3,
	Song_Dungeon4,
	Song_Dungeon5,
	Song_Equipment,
	Song_Evil,
	Song_EvilIntro,
	Song_Magic,
	Song_Night,
	Song_Overcast,
	Song_OverSnow,
	Song_Palace,
	Song_PercIntro,
	Song_Raining,
	Song_Sheet,
	Song_Sneaking,
	Song_Snowing,
	Song_Square,
	Song_SunnyDay,
	Song_Swimming,
	Song_Tavern,
	Song_Temple,
	Song_Town,
	Song_Village,
	Song_Vision,
	Song_WinGame,
	Song_ArraySize
};


enum Elevation_t
{
	Elevation_Trench,
	Elevation_TrenchClimbing,
	Elevation_Floor,
	Elevation_FloorClimbing,
	Elevation_Platform,
	Elevation_Jumping,
	Elevation_FallingIntoTrench,
};

enum CellFlags_t
{
	CellFlag_Raised			= 0x00000020,		// raise floor, bed, table...
	CellFlag_Door			= 0x00000100,
	CellFlag_SecretDoor		= 0x00000200,
	CellFlag_Portal			= 0x00000400,		// portals are used to move between levels
	CellFlag_TriggerMask	= 0x00000700,		// if any of these flags it set, the cell has a trigger
	CellFlag_SignNorth		= 0x00001000,
	CellFlag_SignSouth		= 0x00002000,
	CellFlag_SignEast		= 0x00004000,
	CellFlag_SignWest		= 0x00008000,
	CellFlag_SignMask		= 0x0000F000,		// if any of these flags is set, the cell is hollow, but contains a store sign
	CellFlag_SignShift		= 0x000F0000,		// amount of vertical shift for signs, code expects to do (x >> 16) to get value
	CellFlag_Solid_L0		= 0x00100000,		// the Solid_L# flags indicate whether cell is solid on this level
	CellFlag_Solid_L1		= 0x00200000,
	CellFlag_Solid_L2		= 0x00400000,
	CellFlag_Solid_L3		= 0x00800000,
	CellFlag_Transparent	= 0x01000000,		// used for hedge mazes
};


struct CellNode_t
{
public:
	DWORD Flags;
	DWORD Trigger;
	WORD  Texture[4];
};


struct GridCoords_t
{
	long x;
	long z;
	long luma;
};

enum TriggerFlags_t
{
	TriggerFlag_Open		= 0x00000001,		// only valid for doors
	TriggerFlag_Opening		= 0x00000002,
	TriggerFlag_Closing		= 0x00000004,
	TriggerFlag_StateMask	= 0x0000000F,
};


struct Trigger_t
{
	DWORD Flags;
	DWORD East;
	DWORD South;
	DWORD BaseTime;
};


enum SpriteFlags_t
{
	SpriteFlag_Active		= 0x00000001,
	SpriteFlag_Animated		= 0x00000002,
	SpriteFlag_Loot			= 0x00000004,
};


struct Sprite_t
{
	DWORD Flags;
	long  Position[3];
	DWORD TextureID;		// set to zero if sprite is disabled
	DWORD Brightness;		// non-zero for light-casting sprites
	DWORD BaseTime;			// base time in milliseconds used for animation cycle
	long  Width;
	long  Height;
	char  Name[32];
};


struct Projectile_t
{
	long  Position[3];
	long  DeltaX;
	long  DeltaZ;
	long  Brightness;
	DWORD BulletID;
	DWORD ExplodeID;
	DWORD ExplodeSound;
	DWORD BaseTime;
	DWORD Active;			// 0 == inactive, 1 == flying, 2 == exploding
};


struct LightSource_t
{
	long  Position[3];
	long  BaseBrightness;
	long  CurrentBrightness;
	DWORD FlickerTime;
	DWORD FlickerDuration;
};


enum ObjectFlags_t
{
	ObjectFlag_Usable,
};


struct ObjectInfo_t
{
	DWORD Flags;
	char  Name[32];
	DWORD Condition;		// amount of wear on weapons/armor
	DWORD Value;
};


enum CritterFlag_t
{
	CritterFlag_Translucent		= 0x00000001,
	CritterFlag_CorpsePersists	= 0x00000002,
};


#define c_CritterTypeCount	25

struct CritterInfo_t
{
	char* FileName;
	char* DisplayName;
	DWORD Flags;
	long  Center[6];		// pixel offset from left of image to centerline of critter
	long  Vertical[6];		// vertical displacement
	long  Scale[6];
	DWORD VocalizeID;
};


enum CritterState_t
{
	CritterState_FreeSlot	= 0,
	CritterState_Dead		= 1,
	CritterState_Dying		= 2,
	CritterState_Alive		= 3,
};


struct Critter_t
{
	long  Position[3];
	DWORD CritterID;
	long  Direction;
	DWORD State;
	DWORD BaseTime;
	DWORD VocalizeTime;
	char  Name[32];
};


#define QuadFlag_CornerMask		0x0000000F
#define QuadFlag_Ngon			0x00000010		// 
#define QuadFlag_Sprite			0x00000020		// always perpendicular to line of sight -- all pixels have same Z
#define QuadFlag_Translucent	0x00000040		// used to fade background colors by ghost and wraith sprites
#define QuadFlag_Transparent	0x00000080
#define QuadFlag_ZBiasCloser	0x00000100		// set this flag to bias sorting depth closer -- used for stencils and doors
#define QuadFlag_ZBiasFarther	0x00000200		// set this flag to bias sorting depth farther away -- used for sides of abyss


struct Quad_t
{
	DWORD		ShapeID;
	DWORD		Flags;
	long		Depth;
	CwaTexture*	pTexture;
	Vertex_t	Corners[5];						// quads should have 4 corners, but after near-z clipping may have 3 or 5
};


struct QuadSort_t
{
	DWORD		Index;
	long		Depth;
};


struct Triangle_t
{
	Triangle_t* pNext;
	Vertex_t    Corners[3];
};


struct SoundClip_t
{
	DWORD  SoundID;
	DWORD  SampleCount;
	short* pSamples;
};


struct SoundFile_t
{
	char*     Name;
	SoundID_t ID;
	DWORD     FilterLength;
	DWORD     TrimStart;		// some clip have garbage at the beginning -- zero-out this many samples
};


struct SongFile_t
{
	char*    Name;
	char*    MidiName;
	SongID_t ID;
};


struct AnimInfo_t
{
	AnimationName_t ID;
	char* Name;
	long  Scale;
	DWORD LoopDuration;			// milliseconds required to cycle through animation sequence
	DWORD Brightness;			// 0-100, where 0 == sprite/anim does not emit light
};


enum FlatType_t
{
	Flat_Unused,				// unused, undefined, or otherwise unknown
	Flat_Static,				// static image, usually .IMG file extension
	Flat_Anim,					// animated image, .DFA file
	Flat_Loot,					// static image, with inventory list
	Flat_Critter,
};

// List of flats from INF file.  A flat may be a static object, loot pile,
// monster, animation, etc.
struct FlatInfo_t
{
	FlatType_t	Type;
	DWORD		Index;
	bool		IsAnimated;
	bool		IsLarge;
	long		LightLevel;
	long		OffsetY;		// vertical offset used for objects hanging from ceiling
};


struct SpellInfo_t
{
	char* Name;
	DWORD AnimationID;
	DWORD BulletID;
	DWORD ExplodeID;
	DWORD ExplodeSound;
	DWORD Special;
};


struct Character_t
{
	char Name[32];
	char Race[32];
	char Class[32];
	long Strength;
	long Intelligence;
	long Willpower;
	long Agility;
	long Speed;
	long Endurance;
	long Personality;
	long Luck;

	long HealthCurrent;
	long HealthMaximum;
	long FatigueMaximum;
	long FatigueCurrent;
	long Gold;
	long Experience;
	long Level;

	long DamageModifier;
	long SpellPointsMaximum;
	long SpellPointsCurrent;
	long MagicDefence;
	long ToHitModifier;
	long HealthModifier;
	long CharismaModifier;
	long MaxWeight;
	long ArmorClassModifier;
	long HealModifier;
};

