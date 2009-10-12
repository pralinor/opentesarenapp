/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Profiler.cpp 2     12/31/06 10:52a Lee $
//
//	File: Profiler.cpp
//
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"


struct ProfileEntry_t
{
	DWORD ID;
	char* Name;
};


ProfileEntry_t g_ProfileTable[PFID_ArraySize] =
{
	{ PFID_FixedDistance,						"FixedDistance()" },
	{ PFID_FixedSqrt,							"FixedSqrt()" },
	{ PFID_Main,								"main()" },
	{ PFID_BSALoadResources,					"BSALoadResources()" },
	{ PFID_Load_Animation,						"LoadAnimation()" },
	{ PFID_Load_CIF,							"LoadCIF()", },
	{ PFID_Load_IMG,							"LoadIMG()", },
	{ PFID_Load_Sound,							"LoadSound()" },
	{ PFID_Load_WallTexture,					"LoadWallTexture()" },
	{ PFID_Load_Water,							"LoadWater()" },
	{ PFID_World_AddHorizontalQuad,				"World::AddHorizontalQuad()" },
	{ PFID_World_AddVerticalQuad,				"World::AddVerticalQuad()" },
	{ PFID_World_ClipQuadToNearZ,				"World::ClipQuadToNearZ()" },
	{ PFID_World_ComputeBrightness,				"World::ComputeBrightness()" },
	{ PFID_World_Load,							"World::Load()" },
	{ PFID_World_MovePlayer,					"World::MovePlayer()" },
	{ PFID_World_PushPlayerBack,				"World::PushPlayerBack()" },
	{ PFID_World_Render,						"World::Render()" },
	{ PFID_World_RenderCell,					"World::RenderCell()" },
	{ PFID_World_RenderCritter,					"World::RenderCritter()" },
	{ PFID_World_RenderGameScreen,				"World::RenderGameScreen()" },
	{ PFID_World_RenderProjectile,				"World::RenderProjectile()" },
	{ PFID_World_RenderQuadList,				"World::RenderQuadList()" },
	{ PFID_World_RenderQuadListWithDepth,		"World::RenderQuadListWithDepth()" },
	{ PFID_World_RenderSpellCasting,			"World::RenderSpellCasting()" },
	{ PFID_World_RenderSprite,					"World::RenderSprite()" },
	{ PFID_World_RenderWater,					"World::RenderWater()" },
	{ PFID_World_RenderWeaponOverlay,			"World::RenderWeaponOverlay()" },
	{ PFID_World_ResampleAudio,					"World::ResampleAudio()" },
	{ PFID_World_SortQuadList,					"World::SortQuadList()" },
	{ PFID_World_TransformVertices,				"World::TransformVertices()" },
	{ PFID_World_Unload,						"World::Unload()" },
	{ PFID_World_UpdateLightSources,			"World::UpdateLightSources()" },
	{ PFID_Raster_Blit,							"Raster::Blit()" },
	{ PFID_Raster_BlitRect,						"Raster::BlitRect()" },
	{ PFID_Raster_BlitTransparent,				"Raster::BlitTransparent()" },
	{ PFID_Raster_BlitTransparentMirror,		"Raster::BlitTransparentMirror()" },
	{ PFID_Raster_DisplayFrame,					"Raster::DisplayFrame()" },
	{ PFID_Raster_FillRect,						"Raster::FillRect()" },
	{ PFID_Raster_PaintNgon,					"Raster::PaintNgon()" },
	{ PFID_Raster_PaintNgonFast,				"Raster::PaintNgonFast()" },
	{ PFID_Raster_PaintSprite,					"Raster::PaintSprite()" },
	{ PFID_Raster_PaintSpriteTranslucent,		"Raster::PaintSpriteTranslucent()" },
	{ PFID_Raster_PaintVerticalQuad,			"Raster::PaintVerticalQuad()" },
	{ PFID_Raster_PaintVerticalQuadTransparent,	"Raster::PaintVerticalQuadTransparent()" },
	{ PFID_Raster_PaintVerticalQuadDepthID,		"Raster::PaintVerticalQuadDepthID()" },
};


__int64	g_ProfileTimeAccum[PFID_ArraySize];
__int64	g_ProfileTimeMax[PFID_ArraySize];
DWORD	g_ProfileUseCount[PFID_ArraySize];


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaProfilerManager::CwaProfilerManager(void)
	:	m_pDummyProfiler(NULL)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaProfilerManager::~CwaProfilerManager(void)
{
	// Should have already been destroyed by SaveReport(), but do this anyway
	// since the program may have had to shut down prematurely.
	SafeDelete(m_pDummyProfiler);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Initialize()
//
void CwaProfilerManager::Initialize(void)
{
	SafeDelete(m_pDummyProfiler);

	m_pDummyProfiler = new CwaProfiler(PFID_Main);

	for (DWORD i = 0; i < PFID_ArraySize; ++i) {
		assert(i == g_ProfileTable[i].ID);

		g_ProfileTimeAccum[i] = 0;
		g_ProfileTimeMax[i]   = 0;
		g_ProfileUseCount[i]  = 0;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SaveReport()
//
void CwaProfilerManager::SaveReport(char filename[])
{
	SafeDelete(m_pDummyProfiler);

	FILE *pFile = fopen(filename, "w");

	if (NULL == pFile) {
		return;
	}

	DWORD i;

	__int64 totalTime = 0;

	for (i = 0; i < PFID_ArraySize; ++i) {
		totalTime += g_ProfileTimeAccum[i];
	}

	__int64 frequency;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&frequency));

	fprintf(pFile, "   Total       Max        Avg     Fraction    Count\n");
	fprintf(pFile, " ---------   --------   --------  --------    -----\n");

	for (i = 0; i < PFID_ArraySize; ++i) {
		DWORD totUS  = DWORD((__int64(1000000) * g_ProfileTimeAccum[i]) / frequency);
		DWORD totSec = totUS / 1000000;

		DWORD maxUS  = DWORD((__int64(1000000) * g_ProfileTimeMax[i]) / frequency);
		DWORD maxSec = maxUS / 1000000;

		DWORD count  = max(1, g_ProfileUseCount[i]);
		DWORD avgUS  = DWORD((__int64(1000000) * (g_ProfileTimeAccum[i] / __int64(count))) / frequency);
		DWORD avgSec = avgUS / 1000000;

		totUS %= 1000000;
		maxUS %= 1000000;
		avgUS %= 1000000;

		DWORD fraction = DWORD((__int64(1000000) * g_ProfileTimeAccum[i]) / totalTime);

		fprintf(pFile, "%3d.%06d %3d.%06d %3d.%06d  0.%06d %8d  %s\n",
			totSec, totUS,
			maxSec, maxUS,
			avgSec, avgUS,
			fraction,
			g_ProfileUseCount[i],
			g_ProfileTable[i].Name);
	}

	fclose(pFile);
}


