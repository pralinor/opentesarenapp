/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/WorldBuilder.cpp 4     12/31/06 11:22a Lee $
//
//	File: WorldBuilder.cpp
//
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"
#include "World.h"


extern AnimInfo_t    g_AnimInfo[];
extern CritterInfo_t g_CritterInfo[];


/////////////////////////////////////////////////////////////////////////////
//
//	ResetLevel()
//
void CwaWorld::ResetLevel(void)
{
	memset(m_Critter,    0, sizeof(m_Critter));
	memset(m_Lights,     0, sizeof(m_Lights));
	memset(m_Projectile, 0, sizeof(m_Projectile));
	memset(m_Sprite,     0, sizeof(m_Sprite));
	memset(m_Triggers,   0, sizeof(m_Triggers));

	m_TriggerCount = 0;
	m_LightCount   = 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetCellTrigger()
//
void CwaWorld::SetCellTrigger(DWORD south, DWORD east, DWORD type)
{
	if (m_TriggerCount < ArraySize(m_Triggers)) {
		DWORD triggerNum = m_TriggerCount++;

		m_Triggers[triggerNum].Flags	= type;
		m_Triggers[triggerNum].East		= east;
		m_Triggers[triggerNum].South	= south;
		m_Triggers[triggerNum].BaseTime	= 0;

		m_Nodes[south][east].Flags    |= type;
		m_Nodes[south][east].Trigger   = triggerNum;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PortalAngle()
//
long CwaWorld::PortalAngle(Trigger_t &trigger)
{
	long angle = 0;

	if (TriggerFlag_Open & trigger.Flags) {
		angle = c_FixedPointOne;
	}
	else if (TriggerFlag_Opening & trigger.Flags) {
		angle = ((m_CurrentTime - trigger.BaseTime) * c_FixedPointOne) / c_PortalOpenTime;
	}
	else if (TriggerFlag_Closing & trigger.Flags) {
		long delta = c_PortalOpenTime - (m_CurrentTime - trigger.BaseTime);

		angle = (delta * c_FixedPointOne) / c_PortalOpenTime;
	}

	if (angle < 0) {
		angle = 0;
	}
	else if (angle > c_FixedPointOne) {
		angle = c_FixedPointOne;
	}

	return angle;
}


/////////////////////////////////////////////////////////////////////////////
//
//	AllocCritter()
//
DWORD CwaWorld::AllocCritter(void)
{
	for (DWORD i = 0; i < ArraySize(m_Critter); ++i) {
		if (CritterState_FreeSlot == m_Critter[i].State) {
			return i;
		}
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	AllocLight()
//
DWORD CwaWorld::AllocLight(void)
{
	if ((m_LightCount + 1) < ArraySize(m_Lights)) {
		return m_LightCount++;
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	AllocSprite()
//
DWORD CwaWorld::AllocSprite(void)
{
	for (DWORD i = 0; i < ArraySize(m_Sprite); ++i) {
		if (0 == (SpriteFlag_Active & m_Sprite[i].Flags)) {
			return i;
		}
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//	FreeSprite()
//
void CwaWorld::FreeSprite(DWORD spriteID)
{
	if (spriteID < ArraySize(m_Sprite)) {
		m_Sprite[spriteID].Flags &= ~SpriteFlag_Active;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	AddAnimatedSprite()
//
DWORD CwaWorld::AddAnimatedSprite(DWORD anim, long position[3])
{
	DWORD id = AllocSprite();

	DWORD animID = g_AnimInfo[anim].ID;

	m_Sprite[id].Flags       = SpriteFlag_Active | SpriteFlag_Animated;
	m_Sprite[id].TextureID   = animID;
	m_Sprite[id].Brightness  = 0;
	m_Sprite[id].BaseTime    = 0;
	m_Sprite[id].Width       = IntToFixed(m_Animations[animID].m_Width)  / g_AnimInfo[animID].Scale;
	m_Sprite[id].Height      = IntToFixed(m_Animations[animID].m_Height) / g_AnimInfo[animID].Scale;
	m_Sprite[id].Position[0] = position[0];
	m_Sprite[id].Position[1] = position[1];
	m_Sprite[id].Position[2] = position[2];

	strcpy(m_Sprite[id].Name, g_AnimInfo[anim].Name);

	if (g_AnimInfo[anim].Brightness > 0) {
		DWORD lightNum = AllocLight();

		m_Lights[lightNum].Position[0]       = position[0];
		m_Lights[lightNum].Position[1]       = position[1];
		m_Lights[lightNum].Position[2]       = position[2];
		m_Lights[lightNum].BaseBrightness    = (g_AnimInfo[anim].Brightness * c_FixedPointMask) / 100;
	}

	return id;
}


/////////////////////////////////////////////////////////////////////////////
//
//	MakeCritter()
//
DWORD CwaWorld::MakeCritter(DWORD critterID, long position[3])
{
	DWORD id = AllocCritter();

	m_Critter[id].CritterID		= critterID;
	m_Critter[id].Direction		= 0;
	m_Critter[id].State			= CritterState_Alive;
	m_Critter[id].BaseTime		= 0;
	m_Critter[id].VocalizeTime	= 5000 + (rand() % 5000);
	m_Critter[id].Position[0]	= position[0];
	m_Critter[id].Position[1]	= position[1];
	m_Critter[id].Position[2]	= position[2];

	strcpy(m_Critter[id].Name, g_CritterInfo[critterID].DisplayName);

	return id;
}


/////////////////////////////////////////////////////////////////////////////
//
//	MakeFlatSprite()
//
DWORD CwaWorld::MakeFlatSprite(DWORD textureID, long position[3], DWORD scale)
{
	DWORD id = AllocSprite();

	CwaTexture &texture = m_FlatList[textureID];

	assert(NULL != texture.m_pTexels);

	m_Sprite[id].Flags       = SpriteFlag_Active;
	m_Sprite[id].TextureID   = textureID;
	m_Sprite[id].Brightness  = 0;
	m_Sprite[id].BaseTime    = 0;
	m_Sprite[id].Width       = IntToFixed(scale * texture.m_Width)  >> 7;
	m_Sprite[id].Height      = IntToFixed(scale * texture.m_Height) >> 7;
	m_Sprite[id].Position[0] = position[0];
	m_Sprite[id].Position[1] = position[1];
	m_Sprite[id].Position[2] = position[2];

	strcpy(m_Sprite[id].Name, "flat");

	return id;
}


/////////////////////////////////////////////////////////////////////////////
//
//	MakeLootPile()
//
DWORD CwaWorld::MakeLootPile(DWORD textureID, long position[3])
{
	DWORD id = MakeFlatSprite(textureID, position, 1);

	m_Sprite[id].Flags |= SpriteFlag_Loot;

	strcpy(m_Sprite[id].Name, "loot pile");

	return id;
}


/////////////////////////////////////////////////////////////////////////////
//
//	BuildTestLevel()
//
#if 0
void CwaWorld::BuildTestLevel(void)
{
	ResetLevel();

	DWORD i;

	for (DWORD south = 0; south < 40; ++south) {
		for (DWORD east = 0; east < 40; ++east) {
			m_Nodes[south][east].Flags		= 0;
			m_Nodes[south][east].Texture[0]	= Texture_WoodPlank;
			m_Nodes[south][east].Texture[1]	= Texture_WallB2;
			m_Nodes[south][east].Texture[2]	= Texture_Marble;
			m_Nodes[south][east].Texture[3]	= Texture_Marble;
			m_Nodes[south][east].Trigger	= 0;
		}
	}

	m_Nodes[13][15].Volume[1] = 0;
	m_Nodes[14][15].Volume[1] = 0;
	m_Nodes[15][15].Volume[1] = 0;
	m_Nodes[16][15].Volume[1] = 0;
	m_Nodes[17][15].Volume[1] = 0;

	m_Nodes[13][16].Volume[1] = 0;
	m_Nodes[14][16].Volume[1] = 0;
	m_Nodes[15][16].Volume[1] = 0;
	m_Nodes[16][16].Volume[1] = 0;
	m_Nodes[17][16].Volume[1] = 0;

	m_Nodes[16][17].Volume[1] = 0;
	m_Nodes[13][17].Volume[1] = 0;

	m_Nodes[14][18].Volume[1] = 0;
	m_Nodes[13][18].Volume[1] = 0;
	m_Nodes[12][18].Volume[1] = 0;

	m_Nodes[13][19].Volume[1] = 0;
	m_Nodes[12][19].Volume[1] = 0;

	m_Nodes[13][20].Volume[1] = 0;

	m_Nodes[13][21].Volume[1] = 0;

	m_Nodes[15][17].Volume[0] = 0;
	m_Nodes[16][17].Volume[0] = 0;
	m_Nodes[17][17].Volume[0] = 0;

	m_Nodes[17][14].Volume[1] = Texture_Mural1;
	m_Nodes[16][14].Volume[1] = Texture_Mural2;
	m_Nodes[15][14].Volume[1] = Texture_Mural3;
	m_Nodes[14][14].Volume[1] = Texture_Cabinet;
	m_Nodes[12][15].Volume[1] = Texture_GlassSword;
	m_Nodes[12][16].Volume[1] = Texture_GlassDragon;
	m_Nodes[18][15].Volume[1] = Texture_GlassHand;
	m_Nodes[18][16].Volume[1] = Texture_GlassCrown;

	for (DWORD s =  7; s < 36; ++s) {
		for (DWORD e = 20; e < 30; ++e) {
			m_Nodes[s][e].Volume[1] = 0;
		}
	}

	m_Nodes[10][18].Volume[1] = 0;

	m_Nodes[11][18].Volume[1] = Texture_WallB4;
	m_Nodes[13][17].Volume[1] = Texture_Door;
	m_Nodes[13][14].Volume[1] = Texture_Door;
	m_Nodes[10][12].Volume[1] = Texture_Door;
	m_Nodes[ 4][12].Volume[1] = Texture_ShiftGate;

	SetCellTrigger(11, 18, CellFlag_SecretDoor);
	SetCellTrigger(13, 17, CellFlag_Door);
	SetCellTrigger(13, 14, CellFlag_Door);
	SetCellTrigger(10, 12, CellFlag_Door);
	SetCellTrigger( 4, 12, CellFlag_Portal);

	m_Nodes[11][10].Volume[1] = 0;
	m_Nodes[11][11].Volume[1] = 0;
	m_Nodes[11][12].Volume[1] = 0;
	m_Nodes[11][13].Volume[1] = 0;
	m_Nodes[12][10].Volume[1] = 0;
	m_Nodes[12][11].Volume[1] = 0;
	m_Nodes[12][12].Volume[1] = 0;
	m_Nodes[12][13].Volume[1] = 0;
	m_Nodes[13][10].Volume[1] = 0;
	m_Nodes[13][11].Volume[1] = 0;
	m_Nodes[13][12].Volume[1] = 0;
	m_Nodes[13][13].Volume[1] = 0;

	m_Nodes[5][12].Volume[1] = 0;
	m_Nodes[6][12].Volume[1] = 0;

	m_Nodes[7][10].Volume[1] = 0;
	m_Nodes[7][11].Volume[1] = 0;
	m_Nodes[7][12].Volume[1] = 0;
	m_Nodes[7][13].Volume[1] = 0;
	m_Nodes[8][10].Volume[1] = 0;
	m_Nodes[8][11].Volume[1] = 0;
	m_Nodes[8][12].Volume[1] = 0;
	m_Nodes[8][13].Volume[1] = 0;
	m_Nodes[9][10].Volume[1] = 0;
	m_Nodes[9][11].Volume[1] = 0;
	m_Nodes[9][12].Volume[1] = 0;
	m_Nodes[9][13].Volume[1] = 0;

	m_MaxEast  = 31;
	m_MaxSouth = 37;


//	m_Position[0] = IntToFixed(22) + c_FixedPointHalf;
//	m_Position[1] = IntToFixed( 1) + c_EyeLevel;
//	m_Position[2] = IntToFixed(13) + c_FixedPointHalf;

	m_Position[0] = IntToFixed(15) + c_FixedPointHalf;
	m_Position[1] = IntToFixed( 1) + c_EyeLevel;
	m_Position[2] = IntToFixed(16) + c_FixedPointHalf;

	SetPlayerDirection(c_FixedPointHalf);

	long position[3];

	const long separation = c_FixedPointOne + c_FixedPointHalf;

	for (i = 0; i < 74; ++i) {
		position[0] = IntToFixed(21) + ((i % 6) * separation);
		position[1] = IntToFixed( 1);
		position[2] = IntToFixed( 9) + ((i / 6) * separation);

		AddAnimatedSprite(g_AnimInfo[i].ID, position);
	}

	position[0] = IntToFixed(16) + c_FixedPointHalf;
	position[1] = IntToFixed( 1);
	position[2] = IntToFixed(17) + c_FixedPointHalf;
/*
	position[2] = IntToFixed(15);
	MakeLootPile(Texture_ChestClosed, position, 0);
	position[2] = IntToFixed(15) + c_FixedPointHalf;
	MakeLootPile(Texture_ChestOpen, position, 0);
	position[2] = IntToFixed(16);
	MakeLootPile(Texture_Equipment, position, 0);
	position[2] = IntToFixed(16) + c_FixedPointHalf;
	MakeLootPile(Texture_Treasure1, position, 0);
	position[2] = IntToFixed(17);
	MakeLootPile(Texture_Treasure2, position, 0);
	position[2] = IntToFixed(17) + c_FixedPointHalf;
	MakeLootPile(Texture_Treasure3, position, 0);
*/
	for (i = 0; i < c_CritterTypeCount; ++i) {
		m_Creatures[i].LoadAnimation(g_CritterInfo[i].FileName);

		m_Critter[i].CritterID		= i;
		m_Critter[i].Direction		= 0;
		m_Critter[i].State			= CritterState_Alive;
		m_Critter[i].BaseTime		= 0;
		m_Critter[i].VocalizeTime	= 5000 + (rand() % 5000);
		m_Critter[i].Position[0]	= IntToFixed(21) + ((i % 6) * separation);
		m_Critter[i].Position[1]	= IntToFixed( 1);
		m_Critter[i].Position[2]	= IntToFixed(29) + ((i / 6) * separation);

		strcpy(m_Critter[i].Name, g_CritterInfo[i].DisplayName);
	}

	m_SkyColor   = 0;
	m_FullBright = false;
}


/////////////////////////////////////////////////////////////////////////////
//
//	BuildTestCity()
//
void CwaWorld::BuildTestCity(void)
{
	ResetLevel();

	DWORD south, east;

	m_MaxEast  = 30;
	m_MaxSouth = 30;

	for (south = 0; south < m_MaxSouth; ++south) {
		for (east = 0; east < m_MaxEast; ++east) {
			m_Nodes[south][east].Flags		= 0;
			m_Nodes[south][east].Volume[0]	= Texture_TCityPaving;
			m_Nodes[south][east].Volume[1]	= Texture_TCityWall;
			m_Nodes[south][east].Volume[2]	= Texture_TCityWall;
			m_Nodes[south][east].Volume[3]	= 0;
			m_Nodes[south][east].Trigger	= 0;
		}
	}

	for (south = 1; south < (m_MaxSouth - 1); ++south) {
		for (east = 1; east < (m_MaxEast - 1); ++east) {
			m_Nodes[south][east].Volume[1] = 0;
			m_Nodes[south][east].Volume[2] = 0;
			m_Nodes[south][east].Volume[3] = 0;
		}
	}

	m_Position[0] = IntToFixed(15) + c_FixedPointHalf;
	m_Position[1] = IntToFixed( 1) + c_EyeLevel;
	m_Position[2] = IntToFixed(16) + c_FixedPointHalf;

	SetPlayerDirection(c_FixedPointHalf);

	m_Nodes[0][10].Volume[1] = Texture_TCityGateLeft;
	m_Nodes[0][11].Volume[1] = Texture_TCityGateRight;

	for (south = 0; south < 10; ++south) {
		m_Nodes[south][10].Volume[0]= Texture_TCityRoad;
		m_Nodes[south][11].Volume[0]= Texture_TCityRoad;
	}

	for (south = 14; south < 20; ++south) {
		for (east = 5; east < 12; ++east) {
			m_Nodes[south][east].Volume[1] = Texture_MageExtWall;
		}
	}

	for (south = 15; south < 19; ++south) {
		for (east = 6; east < 11; ++east) {
			m_Nodes[south][east].Volume[2] = Texture_MageExtWall;
		}
	}

	for (south = 16; south < 18; ++south) {
		for (east = 7; east < 10; ++east) {
			m_Nodes[south][east].Volume[3] = Texture_MageExtWall;
		}
	}

	m_Nodes[14][ 7].Volume[1] = Texture_MageExtSun;
	m_Nodes[14][ 8].Volume[1] = Texture_MageExtMoon;
	m_Nodes[14][ 9].Volume[1] = Texture_MageExtStar;
	m_Nodes[15][11].Volume[1] = Texture_MageExtWindow;
	m_Nodes[16][11].Volume[1] = Texture_MageExtMoon;
	m_Nodes[17][11].Volume[1] = Texture_MageExtDoor;
	m_Nodes[18][11].Volume[1] = Texture_MageExtSun;
	m_Nodes[19][11].Volume[1] = Texture_MageExtWindow;

	long position[3];
	position[0] = IntToFixed(10);
	position[1] = IntToFixed( 1);
	position[2] = IntToFixed( 1) + c_FixedPointHalf;
	AddAnimatedSprite(Animation_Streetlight, position);

	position[0] = IntToFixed(12);
	position[1] = IntToFixed( 1);
	position[2] = IntToFixed( 1) + c_FixedPointHalf;
	AddAnimatedSprite(Animation_Streetlight, position);

	m_SkyColor   = 185;
	m_FullBright = true;
}
#endif


/////////////////////////////////////////////////////////////////////////////
//
//	BuildDemoDungeon()
//
void CwaWorld::BuildDemoDungeon(char datafile[], char info[], DWORD width, DWORD height, bool exterior)
{
	ResetLevel();

	FlatInfo_t flats[128];
	memset(flats, 0, sizeof(flats));

	ParseINF(info, flats);

	for (DWORD south = 0; south < 128; ++south) {
		for (DWORD east = 0; east < 128; ++east) {
			m_Nodes[south][east].Flags		= 0;
			m_Nodes[south][east].Texture[0]	= 1;
			m_Nodes[south][east].Texture[1]	= 1;
			m_Nodes[south][east].Texture[2]	= 1;
			m_Nodes[south][east].Texture[3]	= 1;
			m_Nodes[south][east].Trigger	= 0;

			if (false == exterior) {
				m_Nodes[south][east].Flags |= CellFlag_Solid_L2 | CellFlag_Solid_L3;
			}
		}
	}

	FILE *pFile = fopen(datafile, "rb");
	if (NULL == pFile) {
		return;
	}

	for (DWORD south = 0; south < 128; ++south) {
		WORD line[128];
		fread(line, 1, 256, pFile);
		for (DWORD east = 0; east < width; ++east) {
			WORD flags = line[width - 1 - east];

			if (0x8000 & flags) {
				if (0x9000 == (0xF800 & flags)) {
					// FIXME: Handle hedge mazes, needs to render the solid volume
					// on all sides
					// 0x9100 indicates the cell is not solid and can be walked through, but has a front texture
					// the 0x0100 flag may indicate orientation of the front-facing texture
//					m_Nodes[south][east].Flags     |= CellFlag_Solid_L1 | CellFlag_Transparent;
//					m_Nodes[south][east].Texture[1] = (flags & 0x3F) + 1;
				}
				else if (0xA000 == (0xF800 & flags)) {
					if (0x0080 == (0x00C0 & flags)) {
						m_Nodes[south][east].Flags |= CellFlag_SignSouth;
					}
					else if (0x00C0 == (0x00C0 & flags)) {
						m_Nodes[south][east].Flags |= CellFlag_SignEast;
					}
					else if (0x0040 == (0x00C0 & flags)) {
						m_Nodes[south][east].Flags |= CellFlag_SignWest;
					}
					else {
						m_Nodes[south][east].Flags |= CellFlag_SignNorth;
					}

					m_Nodes[south][east].Flags |= (DWORD(flags) & 0x0600) << 7;

					m_Nodes[south][east].Texture[1] = (flags & 0x3F) + 1;
				}
				else if (0xB000 == (0xFF00 & flags)) {
					if (0 == (0x0080 & flags)) {
						SetCellTrigger(south, east, CellFlag_Door);
					}
					else{
						SetCellTrigger(south, east, CellFlag_SecretDoor);
					}

					m_Nodes[south][east].Texture[1] = (flags & 0x3F) + 1;
				}
				else if (0xE000 != flags) {
					m_Nodes[south][east].Texture[1] = 0;
					long position[3];

					position[0] = IntToFixed(east) + c_FixedPointHalf;
					position[1] = IntToFixed(1);
					position[2] = IntToFixed(south) + c_FixedPointHalf;

//					AddAnimatedSprite(g_AnimInfo[i].ID, position);
					DWORD flatNum = flags & 0xFF;
					position[1] += flats[flatNum].OffsetY;

					if (Flat_Static == flats[flatNum].Type) {
						MakeFlatSprite(flats[flatNum].Index, position, flats[flatNum].IsLarge ? 2 : 1);
					}
					else if (Flat_Loot == flats[flatNum].Type) {
						MakeLootPile(flats[flatNum].Index, position);
					}
					else if (Flat_Critter == flats[flatNum].Type) {
						MakeCritter(flats[flatNum].Index, position);
					}
					else if (Flat_Anim == flats[flatNum].Type) {
						AddAnimatedSprite(flats[flatNum].Index, position);
					}

					if (flats[flatNum].LightLevel > 0) {
						DWORD lightNum = AllocLight();
						m_Lights[lightNum].Position[0]       = position[0];
						m_Lights[lightNum].Position[1]       = position[1];
						m_Lights[lightNum].Position[2]       = position[2];
						m_Lights[lightNum].BaseBrightness    = flats[flatNum].LightLevel;
					}
				}
			}
			else if (0x1800 == (flags & 0xFF00)) {
				m_Nodes[south][east].Flags     |= CellFlag_Raised;
				m_Nodes[south][east].Texture[1] = 0;
			}
			else if (0 == flags) {
				m_Nodes[south][east].Texture[1] = 0;
			}
			else if ((flags >> 8) == (flags & 0xFF)) {
				m_Nodes[south][east].Flags     |= CellFlag_Solid_L1;
				m_Nodes[south][east].Texture[1] = (flags & 0x3F) + 1;
			}
			else {
				m_Nodes[south][east].Texture[1] = 0;
			}
		}
	}

	for (DWORD south = 0; south < 128; ++south) {
		WORD line[128];
		fread(line, 1, 256, pFile);
		for (DWORD east = 0; east < width; ++east) {
			WORD flags = line[width - 1 - east];

			if ((flags & 0xFF00) == 0x0D00) {
				m_Nodes[south][east].Texture[0] = 0;
			}
			else {
				m_Nodes[south][east].Flags     |= CellFlag_Solid_L0;
				m_Nodes[south][east].Texture[0] = ((flags >> 8) & 0x3F) + 1;
			}
		}
	}

	for (DWORD i = 0; i < c_CritterTypeCount; ++i) {
		m_Creatures[i].LoadAnimation(g_CritterInfo[i].FileName);
	}

	if (exterior) {
		for (DWORD south = 0; south < 128; ++south) {
			WORD line[128];
			fread(line, 1, 256, pFile);
			for (DWORD east = 0; east < width; ++east) {
				WORD flags = line[width - 1 - east];

				if (0 == flags) {
					m_Nodes[south][east].Texture[2] = 0;
				}
				else {
					m_Nodes[south][east].Flags     |= CellFlag_Solid_L2;
					m_Nodes[south][east].Texture[2] = (flags & 0x3F) + 1;
				}
			}
		}
	}


	fclose(pFile);

	m_MaxEast  = width;
	m_MaxSouth = height;

	// Start off facing east.
	SetPlayerDirection(c_FixedPointOne / 4);

	// This is a hack.  Need to pick starting location specific to which
	// demo level is being loaded.
	if (exterior) {
		m_Position[0] = IntToFixed( 4) + c_FixedPointHalf;
		m_Position[1] = IntToFixed( 1) + c_EyeLevel;
		m_Position[2] = IntToFixed( 5) + c_FixedPointHalf;

		m_SkyColor   = 185;
		m_FullBright = true;
	}
	else {
		m_Position[0] = IntToFixed(48) + c_FixedPointHalf;
		m_Position[1] = IntToFixed( 1) + c_EyeLevel;
		m_Position[2] = IntToFixed(26) + c_FixedPointHalf;

		m_SkyColor   = 0;
		m_FullBright = false;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ParseInfoFlags()
//
void CwaWorld::ParseInfoFlags(char *p, FlatInfo_t &flat)
{
	flat.IsAnimated = false;
	flat.IsLarge    = false;
	flat.LightLevel = 0;
	flat.OffsetY    = 0;

	do {
		// Skip any white space before flags.
		while ((' ' == *p) || ('\t' == *p)) {
			++p;
		}

		if ('\0' == p[0]) {
			return;
		}

		if (':' == p[1]) {
			if (('F' == *p) || ('f' == *p)) {
				DWORD flag = p[2] - '0';

				// This is always used with DFAs, so it's also redundant.
				// (original Arena code may have ignored file extensions)
				if (0x01 & flag) {
					flat.IsAnimated = true;
				}

				// This flag only seems to be used for trees, which need to
				// be drawn much larger (about 4x default size).
				if (0x04 & flag) {
					flat.IsLarge = true;
				}

				if (0x40 & flag) {
					// unknown
					// "sdent1.img" "shaym.img", chair
				}

				// 10 == flag, indicates puddle, drawn using sky color
			}

			// Light level.  Varies from 2 to 6.
			// Candles are usually 2, sometimes 3.
			// Braziers are 3 to 5.
			// Torch posts 4 or 5, very rarely will be 6.
			//
			// This c
			//
			else if (('S' == *p) || ('s' == *p)) {
				// Ignore values larger than 5 (even though 6 does occur rarely).
				long level = atol(p+2);
				level = ClampRange(0, level, 5);
				flat.LightLevel = (level * c_FixedPointMask) / 5;
			}

			// Y-axis offset (where +Y is down)
			else if (('Y' == *p) || ('y' == *p)) {
				long offsetY = atol(p+2);
				flat.OffsetY = -IntToFixed(offsetY) / 128;
			}
		}

		// Skip over this flag field, stopping when we hit white space
		// or the end of the line.
		while ((' ' != *p) && ('\t' != *p) && ('\0' != *p)) {
			++p;
		}
	} while ('\0' != *p);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ParseINF()
//
void CwaWorld::ParseINF(char name[], FlatInfo_t flats[])
{
	DWORD size    = 0;
	bool  fromBSA = false;

	BYTE *pData = m_Loader.LoadFile(name, size, &fromBSA);

	if ((NULL == pData) || (0 == size)) {
		return;
	}

	BYTE *pCopy = new BYTE[size+1];
	memcpy(pCopy, pData, size);

	// The INF files stored in the Arena directory are all plan text.  Those
	// in Globals.BSA have been obfuscated with bit transforms, and will need
	// to be converted to plaintext.
	if (fromBSA) {
		m_Loader.DeobfuscateINF(pCopy, size);
	}

	pCopy[size] = 0;

	char  line[256];
	DWORD offset    = 0;
	DWORD lineIndex = 0;
	bool  doingWalls = true;
	bool  doingFlats = false;
	bool  doingText  = false;

	DWORD textureIndex = 1;
	DWORD flatIndex    = 0;
	DWORD lootNumber   = 0;		// 0 == not a loot pile

	while (offset <= size) {
		bool process = false;

		if ('\0' == pCopy[offset]) {
			process = true;
		}
		else if ('\r' == pCopy[offset]) {
			// no-op
		}
		else if ('\n' == pCopy[offset]) {
			process = true;
		}
		else {
			line[lineIndex++] = pCopy[offset];
		}

		if (process && (lineIndex > 0)) {
			// Zero out the rest of the buffer to avoid any issues with
			// scanning past the end of the line.  This allows assumptions that
			// any flag fields at the end of the line are properly formatted.
			// Otherwise more error checking would be needed there.
			for (DWORD i = lineIndex; i < ArraySize(line); ++i) {
				line[i] = '\0';
			}

			if (('*' == line[0]) || ('@' == line[0])) {
				if (0 == strncmp(line, "@FLATS", 6)) {
					doingWalls = false;
					doingFlats = true;
					doingText  = false;
				}
				if (0 == strncmp(line, "@FLOORS", 7)) {
				}
				if (0 == strncmp(line, "@SOUND", 6)) {
				}
				if (0 == strncmp(line, "@TEXT", 5)) {
					doingWalls = false;
					doingFlats = false;
					doingText  = true;
				}
				if (0 == strncmp(line, "@WALLS", 6)) {
					// FIXME:
					++textureIndex;
				}
				if (0 == strncmp(line, "*BOXCAP", 7)) {
				}
				if (0 == strncmp(line, "*BOXSIDE", 8)) {
				}
				if (0 == strncmp(line, "*DOOR", 5)) {
				}
				if (0 == strncmp(line, "*DRYCHASM", 9)) {
				}
				if (0 == strncmp(line, "*ITEM", 5)) {
					lootNumber = atol(line + 6);
				}
				if (0 == strncmp(line, "*LAVACHASM", 10)) {
				}
				if (0 == strncmp(line, "*LEVELDOWN", 10)) {
					m_LevelDownID = textureIndex;
				}
				if (0 == strncmp(line, "*LEVELUP", 8)) {
					m_LevelUpID = textureIndex;
				}
				if (0 == strncmp(line, "*MENU", 5)) {
				}
				if (0 == strncmp(line, "*TEXT", 5)) {
				}
				if (0 == strncmp(line, "*TRANS", 6)) {
				}
				if (0 == strncmp(line, "*TRANSWALKTHRU", 14)) {
				}
				if (0 == strncmp(line, "*WETCHASM", 9)) {
				}
			}
			else if (doingWalls) {
				char name[64];
				DWORD count = 1;
				sscanf(line, "%s #%d", name, &count);

				for (DWORD i = 0; i < count; ++i) {
					m_WallList[textureIndex++].LoadWallTexture(name, i, true);
				}
			}
			else if (doingFlats) {

// FIXME: hack to get lights working in cities
if (0 == strncmp("nlamp1da.img", line, strlen("nlamp1da.img"))) {
	strcpy(line, "nlamp1.dfa f:1 s:4");
}
if (0 == strncmp("slamp1da.img", line, strlen("slamp1da.img"))) {
	strcpy(line, "slamp1.dfa f:1 s:4");
}

				char name[64];
				char ext[16];

				char *n = name;
				char *p = line;
				while (*p != '.') {
					*(n++) = *(p++);
				}
				*n = '\0';

				ext[0] = p[1];
				ext[1] = p[2];
				ext[2] = p[3];
				ext[3] = '\0';

				p += 4;

				ParseInfoFlags(p, flats[flatIndex]);

				flats[flatIndex].Type = Flat_Unused;

				char fullName[64];
				sprintf(fullName, "%s.%s", name, ext);

				if (0 == stricmp("img", ext)) {
					m_FlatList[flatIndex].LoadIMG(fullName, true);
					assert(NULL != m_FlatList[flatIndex].m_pTexels);

					flats[flatIndex].Type    = (lootNumber > 0) ? Flat_Loot : Flat_Static;
					flats[flatIndex].Index   = flatIndex;
				}
				else if (0 == stricmp("dfa", ext)) {
					DWORD critterID = 0xFFFFFFFF;
					for (DWORD i = 0; i < c_CritterTypeCount; ++i) {
						if (0 == stricmp(g_CritterInfo[i].FileName, name)) {
							critterID = i;
							break;
						}
					}

					if (critterID < c_CritterTypeCount) {
						flats[flatIndex].Type  = Flat_Critter;
						flats[flatIndex].Index = critterID;
					}
					else {
						DWORD animID = 0xFFFFFFFF;
						for (DWORD i = 0; i <= Animation_WaterStatue2; ++i) {
							if (0 == stricmp(g_AnimInfo[i].Name, fullName)) {
								animID = i;
								break;
							}
						}

						if (animID <= Animation_WaterStatue2) {
							flats[flatIndex].Type  = Flat_Anim;
							flats[flatIndex].Index = animID;
						}
					}
				}

				flatIndex += 1;
				lootNumber = 0;
			}
		}

		if (process) {
			lineIndex = 0;
		}

		++offset;
	}

	SafeDeleteArray(pCopy);
}



