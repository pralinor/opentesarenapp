/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Profiler.h 2     12/31/06 10:52a Lee $
//
//	File: Profiler.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


//#define ENABLE_PROFILER

#if defined(ENABLE_PROFILER)
	#define PROFILE(x) CwaProfiler profiler(x)
#else
	#define PROFILE(x)
#endif // !ENABLE_PROFILER


enum ProfileID_t
{
	PFID_FixedDistance,
	PFID_FixedSqrt,
	PFID_Main,
	PFID_BSALoadResources,
	PFID_Load_Animation,
	PFID_Load_CIF,
	PFID_Load_IMG,
	PFID_Load_Sound,
	PFID_Load_WallTexture,
	PFID_Load_Water,
	PFID_World_AddHorizontalQuad,
	PFID_World_AddVerticalQuad,
	PFID_World_ClipQuadToNearZ,
	PFID_World_ComputeBrightness,
	PFID_World_Load,
	PFID_World_MovePlayer,
	PFID_World_PushPlayerBack,
	PFID_World_Render,
	PFID_World_RenderCell,
	PFID_World_RenderCritter,
	PFID_World_RenderGameScreen,
	PFID_World_RenderProjectile,
	PFID_World_RenderQuadList,
	PFID_World_RenderQuadListWithDepth,
	PFID_World_RenderSpellCasting,
	PFID_World_RenderSprite,
	PFID_World_RenderWater,
	PFID_World_RenderWeaponOverlay,
	PFID_World_ResampleAudio,
	PFID_World_SortQuadList,
	PFID_World_TransformVertices,
	PFID_World_Unload,
	PFID_World_UpdateLightSources,
	PFID_Raster_Blit,
	PFID_Raster_BlitRect,
	PFID_Raster_BlitTransparent,
	PFID_Raster_BlitTransparentMirror,
	PFID_Raster_DisplayFrame,
	PFID_Raster_FillRect,
	PFID_Raster_PaintNgon,
	PFID_Raster_PaintNgonFast,
	PFID_Raster_PaintSprite,
	PFID_Raster_PaintSpriteTranslucent,
	PFID_Raster_PaintVerticalQuad,
	PFID_Raster_PaintVerticalQuadTransparent,
	PFID_Raster_PaintVerticalQuadDepthID,
	PFID_ArraySize
};


extern __int64	g_ProfileTimeAccum[];
extern __int64	g_ProfileTimeMax[];
extern DWORD	g_ProfileUseCount[];


/////////////////////////////////////////////////////////////////////////////
//
class CwaProfiler
{
private:
	__int64 m_StartTime;
	DWORD	m_ID;

	CwaProfiler(void) {}

public:
	CwaProfiler(DWORD id)
		:	m_ID(id)
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_StartTime));
	}

	~CwaProfiler(void)
	{
		__int64 sample;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&sample));
		sample -= m_StartTime;
		g_ProfileTimeAccum[m_ID]  += sample;
		if (g_ProfileTimeMax[m_ID] < sample) {
			g_ProfileTimeMax[m_ID] = sample;
		}
		++g_ProfileUseCount[m_ID];
	}
};


/////////////////////////////////////////////////////////////////////////////
//
class CwaProfilerManager
{
private:
	CwaProfiler* m_pDummyProfiler;

public:
	CwaProfilerManager(void);
	~CwaProfilerManager(void);

	void Initialize(void);
	void SaveReport(char filename[]);
};




