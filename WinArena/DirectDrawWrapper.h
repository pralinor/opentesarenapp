/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/DirectDrawWrapper.h 2     12/31/06 10:47a Lee $
//
//	File: DirectDrawWrapper.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#include "ddraw.h"


class CDirectDrawWrapper
{
public:
	IDirectDraw7*			m_pDraw;
	IDirectDrawClipper*		m_pClipper;
	IDirectDrawSurface7*	m_pPrimary;
	IDirectDrawSurface7*	m_pBackBuffer;
	IDirectDrawSurface7*	m_pRenderBuffer[2];
	IDirectDrawPalette*		m_pPalette;

	DWORD	m_ScreenWidth;
	DWORD	m_ScreenHeight;
	DWORD	m_RenderWidth;
	DWORD	m_RenderHeight;

	bool	m_LockedRenderBuffer;
	DWORD	m_RenderIndex;
	DWORD	m_ClearCount;

	char	m_ErrorMessage[256];

	DWORD	m_BlitCount;
	float	m_BlitTime;

	////////////////////////////////////////////////////////////////////////////

	CDirectDrawWrapper(void);
	~CDirectDrawWrapper(void);

	void WriteErrorMessage(char who[], HRESULT hr);

	bool Initialize(HWND hWindow, DWORD width, DWORD height);

	void ClearSurface(IDirectDrawSurface7 *pSurface);
	bool LockRenderBuffer(BYTE **ppMemory, DWORD &pitch);
	void UnlockRenderBuffer(void);
	void ShowRenderBuffer(void);
	void SetPalette(DWORD *pPalette);

	IDirectDrawSurface7* AllocateExisting(BYTE *pBuffer, DWORD Width, DWORD height, DWORD bitDepth);

	void TestBlit(BYTE *pBuffer, long x, long y);

	void LogCapabilities(DDCAPS &caps, FILE *pFile);
	void LogCaps(DWORD flags, FILE *pFile);
	void LogCaps2(DWORD flags, FILE *pFile);
	void LogKeyCaps(DWORD flags, FILE *pFile);
	void LogFxCaps(DWORD flags, FILE *pFile);
	void LogAlphaCaps(DWORD flags, FILE *pFile);
	void LogPaletteCaps(DWORD flags, FILE *pFile);
};

