/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/WinArena.cpp 10    12/31/06 11:10a Lee $
//
//	File: WinArena.cpp
//
//
//	Defines the entry point for the application.
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"
#include "Resource.h"
#include "ParseBSA.h"
#include "Raster.h"
#include "SoundDriver.h"
#include "World.h"
#include "DirectDrawWrapper.h"

CDirectDrawWrapper* g_pDDWrapper = NULL;

// Global Variables:
HINSTANCE	g_hInstance			= NULL;		// current instance
HWND		g_hWindow			= NULL;
HMENU		g_hMenu				= NULL;
TCHAR		g_Title[128];					// The title bar text
TCHAR		g_WindowClass[128];				// the main window class name
DWORD		g_WindowWidth		= 640;
DWORD		g_WindowHeight		= 400;
DWORD		g_ScreenWidth		= 640;		// screen width/height only used in full-screen mode
DWORD		g_ScreenHeight		= 480;
DWORD		g_ScreenShotIndex	= 0;
bool		g_EnableFullScreen	= false;
bool		g_EnableDirectDraw	= false;
bool		g_AbortStartup		= false;
CwaRaster*	g_pRaster			= NULL;
CwaWorld*	g_pWorld			= NULL;
bool		g_Minimized			= false;
bool		g_TrackingMouse		= false;	// need to track when mouse is in client window
bool		g_LeftMouse			= false;
bool		g_RightMouse		= false;
bool		g_ShowFPS			= true;
bool		g_Illumination		= true;

bool		g_KeyDownUp			= false;
bool		g_KeyDownLeft		= false;
bool		g_KeyDownDown		= false;
bool		g_KeyDownRight		= false;


/////////////////////////////////////////////////////////////////////////////
//
//	ResetWindowSize()
//
void ResetWindowSize(DWORD newWidth, DWORD newHeight)
{
	g_WindowWidth  = newWidth;
	g_WindowHeight = newHeight;

	RECT clientRect, windowRect, desktopRect;

	GetClientRect(g_hWindow, &clientRect);
	GetWindowRect(g_hWindow, &windowRect);
	GetWindowRect(GetDesktopWindow(), &desktopRect);

	long width  = g_WindowWidth  + (windowRect.right  - windowRect.left) - (clientRect.right  - clientRect.left);
	long height = g_WindowHeight + (windowRect.bottom - windowRect.top)  - (clientRect.bottom - clientRect.top);

	long x = (desktopRect.right  - desktopRect.left) - width;
	long y = (desktopRect.bottom - desktopRect.top)  - height;

	SetWindowPos(g_hWindow, NULL, x/2, y/2, width, height, SWP_NOZORDER);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScreenShot()
//
void ScreenShot(void)
{
	if (NULL != g_pRaster) {

		// Explicitly call Render().  This will guarantee there
		// is a frame in the rasterizer's frame buffer in case
		// the rendering was done somewhere else (such as into
		// a DirectDraw surface).
		g_pWorld->Render();

		// Now find an available filename.  Repeatedly attempt
		// to open existing files until the attempt fails.  That
		// indicates the filename is not in use, so the new
		// screenshot will be saved under that name.

		char name[64];
		bool found = false;

		while (!found) {
			sprintf(name, "screen%03d.gif", g_ScreenShotIndex);
			FILE *pTest = fopen(name, "rb");

			if (NULL != pTest) {
				fclose(pTest);
			}
			else {
				found = true;
			}

			++g_ScreenShotIndex;
		}

		g_pRaster->ScreenShot(name);

		// Display a message confirming file was saved, and
		// what name was used.

		char message[128];
		sprintf(message, "Screenshot saved to %s", name);

		g_pWorld->SetDisplayMessage(message);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	MainWindowProc()
//
//	Processes messages for the main window.
//
LRESULT CALLBACK MainWindowProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hDC;

	switch (message) {
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			// Parse the menu selections:
			switch (wmId) {
				case ID_FILE_SCREENSHOT:
					ScreenShot();
					break;

				case ID_QUALITY_SHOWFPS:
					g_ShowFPS = !g_ShowFPS;
					if (NULL != g_pWorld) {
						g_pWorld->ShowFPS(g_ShowFPS);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_SHOWFPS, g_ShowFPS ? MF_CHECKED : MF_UNCHECKED);
					break;

				case ID_QUALITY_ILLUMINATE:
					g_Illumination = !g_Illumination;
					if (NULL != g_pWorld) {
						g_pWorld->EnablePlayerIllumination(g_Illumination);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_ILLUMINATE, g_Illumination ? MF_CHECKED : MF_UNCHECKED);
					break;

				case ID_QUALITY_RESETWINDOWTO320X200:
					ResetWindowSize(320, 200);
					break;

				case ID_QUALITY_RESETWINDOWTO640X400:
					ResetWindowSize(640, 400);
					break;

				case ID_QUALITY_RESETWINDOWTO960X600:
					ResetWindowSize(960, 600);
					break;

				case ID_QUALITY_320X200:
					if (NULL != g_pRaster) {
						g_pRaster->Allocate(320, 200);
						g_pRaster->SetWindowSize(g_WindowWidth, g_WindowHeight);
					}
					if (NULL != g_pWorld) {
						g_pWorld->SetResolution(320, 200);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_320X200, MF_CHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_640X400, MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_960X600, MF_UNCHECKED);
					break;

				case ID_QUALITY_640X400:
					if (NULL != g_pRaster) {
						g_pRaster->Allocate(640, 400);
						g_pRaster->SetWindowSize(g_WindowWidth, g_WindowHeight);
					}
					if (NULL != g_pWorld) {
						g_pWorld->SetResolution(640, 400);
					}
					if (g_WindowWidth < 640) {
						ResetWindowSize(640, 400);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_320X200, MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_640X400, MF_CHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_960X600, MF_UNCHECKED);
					break;

				case ID_QUALITY_960X600:
					if (NULL != g_pRaster) {
						g_pRaster->Allocate(960, 600);
						g_pRaster->SetWindowSize(g_WindowWidth, g_WindowHeight);
					}
					if (NULL != g_pWorld) {
						g_pWorld->SetResolution(960, 600);
					}
					if (g_WindowWidth < 960) {
						ResetWindowSize(960, 600);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_320X200, MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_640X400, MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_960X600, MF_CHECKED);
					break;

				case ID_QUALITY_FASTBLIT:
					if (NULL != g_pRaster) {
						g_pRaster->SetBlitMode(BLACKONWHITE);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_FASTBLIT,   MF_CHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_SMOOTHBLIT, MF_UNCHECKED);
					break;

				case ID_QUALITY_SMOOTHBLIT:
					if (NULL != g_pRaster) {
						g_pRaster->SetBlitMode(HALFTONE);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_FASTBLIT,   MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_SMOOTHBLIT, MF_CHECKED);
					break;

				case ID_QUALITY_DIST_LO:
					if (NULL != g_pWorld) {
						g_pWorld->SetRenderDistance(16);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_LO,   MF_CHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_MID,  MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_HI,   MF_UNCHECKED);
					break;

				case ID_QUALITY_DIST_MID:
					if (NULL != g_pWorld) {
						g_pWorld->SetRenderDistance(64);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_LO,   MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_MID,  MF_CHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_HI,   MF_UNCHECKED);
					break;

				case ID_QUALITY_DIST_HI:
					if (NULL != g_pWorld) {
						g_pWorld->SetRenderDistance(128);
					}
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_LO,   MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_MID,  MF_UNCHECKED);
					CheckMenuItem(g_hMenu, ID_QUALITY_DIST_HI,   MF_CHECKED);
					break;

				case ID_DUMP_BSAINDEX:
					g_pArenaLoader->DumpBSAIndex();
					break;

				case ID_DUMP_ANIMATIONS:
					g_pArenaLoader->DumpDFA();
					g_pArenaLoader->DumpCIF();
					break;

				case ID_DUMP_CREATURES:
					g_pArenaLoader->DumpCFA();
					break;

				case ID_DUMP_IMAGES:
					g_pArenaLoader->DumpImages();
					break;

				case ID_DUMP_INFO:
					g_pArenaLoader->DumpInfo();
					break;

				case ID_DUMP_MAPS:
					g_pArenaLoader->DumpMaps();
					break;

				case ID_DUMP_MUSIC:
					g_pWorld->DumpMusic();
					break;

				case ID_DUMP_SOUNDS:
					g_pArenaLoader->DumpSounds();
					break;

				case ID_DUMP_TEXTURES:
					g_pArenaLoader->DumpWalls();
					break;

				case IDM_EXIT:
					DestroyWindow(hWindow);
					break;

				default:
					return DefWindowProc(hWindow, message, wParam, lParam);
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_PAINT:
			hDC = BeginPaint(hWindow, &ps);
			EndPaint(hWindow, &ps);
			break;

		case WM_KEYDOWN:
			if (VK_LEFT == wParam) {
				g_KeyDownLeft = true;
			}
			else if (VK_UP == wParam) {
				g_KeyDownUp = true;
			}
			else if (VK_DOWN == wParam) {
				g_KeyDownDown = true;
			}
			else if (VK_RIGHT == wParam) {
				g_KeyDownRight = true;
			}
			else if (VK_ESCAPE == wParam) {
				g_pWorld->PressEscKey();
			}
			else if (VK_F11 == wParam) {
				PostQuitMessage(0);
			}
			else if (VK_F12 == wParam) {
				ScreenShot();
			}
			else if ('J' == wParam) {
				g_pWorld->Jump();
			}
			break;

		case WM_KEYUP:
			if (VK_LEFT == wParam) {
				g_KeyDownLeft = false;
			}
			else if (VK_UP == wParam) {
				g_KeyDownUp = false;
			}
			else if (VK_DOWN == wParam) {
				g_KeyDownDown = false;
			}
			else if (VK_RIGHT == wParam) {
				g_KeyDownRight = false;
			}
			break;

		case WM_LBUTTONDOWN:
			if (NULL != g_pWorld) {
				if (g_pWorld->LeftMouseClick()) {
					if (false == g_RightMouse) {
						SetCapture(g_hWindow);
					}
					g_LeftMouse = true;
				}
			}
			break;

		case WM_LBUTTONUP:
			if (NULL != g_pWorld) {
				g_pWorld->RightMouseClick(false);
			}
			if (false == g_RightMouse) {
				ReleaseCapture();
			}
			g_LeftMouse = false;
			break;

		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			if (NULL != g_pWorld) {
				g_pWorld->RightMouseClick(true);
			}
			if (false == g_LeftMouse) {
				SetCapture(g_hWindow);
			}
			g_RightMouse = true;
			break;

		case WM_RBUTTONUP:
			if (NULL != g_pWorld) {
				g_pWorld->RightMouseClick(false);
			}
			if (false == g_LeftMouse) {
				ReleaseCapture();
			}
			g_RightMouse = false;
			break;

		case WM_MOUSEMOVE:
			if ((NULL != g_pWorld) && (NULL != g_pRaster)) {
				DWORD x = LOWORD(lParam);
				DWORD y = HIWORD(lParam);

				if (short(x) < 0) {
					x = 0;
				}
				else if (x >= g_WindowWidth) {
					x = g_WindowWidth - 1;
				}

				if (short(y) < 0) {
					y = 0;
				}
				else if (y >= g_WindowHeight) {
					y = g_WindowHeight - 1;
				}

				// Transform the mouse position from window coordinates to
				// frame buffer coordinates.
				x = (x * g_pRaster->FrameWidth())  / g_WindowWidth;
				y = (y * g_pRaster->FrameHeight()) / g_WindowHeight;

				g_pWorld->SetMousePosition(x, y);
			}

			// If the mouse is not being tracked, turn tracking back on, and
			// hide the mouse pointer (cursor).  The rasterizer will handle
			// drawing a game-mode dependent pointer through the frame buffer,
			// so we don't want the Windows pointer to also be visible at the
			// same time.
			//
			if (false == g_TrackingMouse) {
				g_TrackingMouse = true;
				TRACKMOUSEEVENT tracker;
				tracker.cbSize      = sizeof(tracker);
				tracker.dwFlags     = TME_LEAVE;
				tracker.hwndTrack   = g_hWindow;
				tracker.dwHoverTime = HOVER_DEFAULT;
				TrackMouseEvent(&tracker);
				ShowCursor(FALSE);
			}
			return DefWindowProc(hWindow, message, wParam, lParam);

		case WM_MOUSELEAVE:
			//
			// Once we receive WM_MOUSELEAVE, tracking is turned off.
			// Have to explicitly call TrackMouseEvent() again to turn
			// it back one.  This will be done in WM_MOUSEMOVE.
			//
			// Need to do this to turn the mouse cursor on again when it
			// moves out of the client area.  This allows the mouse cursor
			// to be visible if the user is trying to click on a menu
			// button or the close button.
			//
			if (g_TrackingMouse) {
				g_TrackingMouse = false;
				ShowCursor(TRUE);
			}
			break;

		case WM_MOUSEWHEEL:
			if (NULL != g_pWorld) {
				g_pWorld->ScrollWheel(short(HIWORD(wParam)) < 0);
			}
			break;

		case WM_SIZE:
			if ((SIZE_MINIMIZED != wParam) && LOWORD(lParam) && HIWORD(lParam)) {
				g_Minimized    = false;
				g_WindowWidth  = LOWORD(lParam);
				g_WindowHeight = HIWORD(lParam);
				if (NULL != g_pRaster) {
					g_pRaster->SetWindowSize(g_WindowWidth, g_WindowHeight);
				}
			}
			else {
				g_Minimized = true;
			}
			break;

		default:
			return DefWindowProc(hWindow, message, wParam, lParam);
	}

	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
//
//	ResolutionDialogProc()
//
//	Processes messages for the resultion selection dialog.
//
LRESULT CALLBACK ResolutionDialogProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
	switch (message) {
		case WM_INITDIALOG:
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_MODE_WINDOW:
					g_EnableFullScreen	= false;

					EndDialog(hWindow, 0);
					return TRUE;

				case IDC_MODE_FULL640:
					g_WindowWidth		=  640;
					g_WindowHeight		=  400;
					g_ScreenWidth		=  640;
					g_ScreenHeight		=  480;
					g_EnableFullScreen	= true;
					g_EnableDirectDraw	= true;

					EndDialog(hWindow, 0);
					return TRUE;

				case IDC_MODE_FULL960:
					g_WindowWidth		=  960;
					g_WindowHeight		=  600;
					g_ScreenWidth		= 1024;
					g_ScreenHeight		=  768;
					g_EnableFullScreen	= true;
					g_EnableDirectDraw	= true;

					EndDialog(hWindow, 0);
					return TRUE;

				case IDCANCEL:
					g_AbortStartup		= true;

					EndDialog(hWindow, 0);
					return TRUE;

			}
			break;
	}

	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
//
//	MyRegisterClass()
//
//	This function and its usage are only necessary if you want this code to
//	be compatible with Win32 systems prior to the 'RegisterClassEx' function
//	that was added to Windows 95. It is important to call this function so
//	that the application will get 'well formed' small icons associated with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize			= sizeof(WNDCLASSEX); 
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WNDPROC(MainWindowProc);
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_WINARENA);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= HBRUSH(COLOR_WINDOW+1);
	wcex.lpszMenuName	= g_EnableFullScreen ? NULL : LPCTSTR(IDC_WINARENA);
	wcex.lpszClassName	= g_WindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}


/////////////////////////////////////////////////////////////////////////////
//
//	InitInstance()
//
//	Saves instance handle and creates main window
//
//	In this function, we save the instance handle in a global variable and
//	create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	// Store instance handle in our global variable
	g_hInstance = hInstance;

	DWORD style = WS_OVERLAPPEDWINDOW;

	if (g_EnableFullScreen) {
		style = WS_POPUP;
	}

	g_hWindow = CreateWindow(g_WindowClass, g_Title, style,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (NULL == g_hWindow) {
		return FALSE;
	}


	ResetWindowSize(g_WindowWidth, g_WindowHeight);

	ShowWindow(g_hWindow, nCmdShow);
	UpdateWindow(g_hWindow);

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
//
//	WinMain()
//
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR    pCommandLine,
                     int       showCommand)
{
#ifdef ENABLE_PROFILER
	CwaProfilerManager profileManager;
	profileManager.Initialize();
#endif

	DialogBox(hInstance, (LPCTSTR)IDD_RESOLUTION, NULL, DLGPROC(ResolutionDialogProc));

	if (g_AbortStartup) {
		return 0;
	}

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, g_Title, ArraySize(g_Title));
	LoadString(hInstance, IDC_WINARENA, g_WindowClass, ArraySize(g_WindowClass));
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, showCommand)) {
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_WINARENA);

	if ((NULL != pCommandLine) && ('\0' != pCommandLine[0])) {
		strcpy(g_ArenaPath, pCommandLine);
	}
	else {
		strcpy(g_ArenaPath, ".\\");
	}

	CwaRaster raster;
	raster.Allocate(g_WindowWidth, g_WindowHeight);
	raster.SetWindowSize(g_WindowWidth, g_WindowHeight);

	CwaWorld *pWorld = new CwaWorld;
	pWorld->SetRaster(&raster);
	pWorld->SetResolution(g_WindowWidth, g_WindowHeight);
	pWorld->ShowFPS(g_ShowFPS);
	pWorld->EnablePlayerIllumination(g_Illumination);
	pWorld->Load();

	CDirectDrawWrapper wrapper;

	if (g_EnableFullScreen && g_EnableDirectDraw) {
		if (false == wrapper.Initialize(g_hWindow, g_ScreenWidth, g_ScreenHeight)) {
			char buffer[512];
			sprintf(buffer, "DirectDraw initialization failed:\n\t%s", wrapper.m_ErrorMessage);
			MessageBox(g_hWindow, buffer, "DirectDraw error", MB_OK | MB_ICONEXCLAMATION);
			return 0;
		}

		g_pDDWrapper = &wrapper;
	}
	else {
		g_hMenu = GetMenu(g_hWindow);

		CheckMenuItem(g_hMenu, ID_QUALITY_SHOWFPS, g_ShowFPS ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(g_hMenu, ID_QUALITY_ILLUMINATE, g_Illumination ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(g_hMenu, ID_QUALITY_640X400, MF_CHECKED);
		CheckMenuItem(g_hMenu, ID_QUALITY_FASTBLIT, MF_CHECKED);
		CheckMenuItem(g_hMenu, ID_QUALITY_DIST_LO, MF_CHECKED);
	}

	g_pRaster = &raster;
	g_pWorld  = pWorld;


	// Main message loop:
	MSG  message;
	BOOL newMessage = true;

	TIMECAPS timeCaps;
	timeGetDevCaps(&timeCaps, sizeof(timeCaps));

	timeBeginPeriod(timeCaps.wPeriodMin);

	DWORD prevTime   = timeGetTime();
	DWORD fpsMarker  = prevTime;
	DWORD frameCount = 0;

	do {
		do {
			newMessage = PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE);

			if (newMessage && (WM_QUIT != message.message)) {
				if (GetMessage(&message, NULL, 0, 0)) {
					if (!TranslateAccelerator(message.hwnd, hAccelTable, &message)) {
						TranslateMessage(&message);
						DispatchMessage(&message);
					}
				}
			}

		} while (newMessage && (WM_QUIT != message.message));

		if ((false == g_Minimized) && (WM_QUIT != message.message)) {
			DWORD curTime   = timeGetTime();
			DWORD timeDelta = curTime - prevTime;

			// Protect against really slow frame rates.  These arise whenever
			// accessing a menu option.  Large differences can result in huge
			// changes to the character position, which breaks the simple
			// collision detection/response code.
			if (timeDelta > 250) {
				timeDelta = 250;
			}

			if (g_KeyDownLeft) {
				pWorld->SetTurnSpeed(-1);
			}
			else if (g_KeyDownRight) {
				pWorld->SetTurnSpeed(+1);
			}
			else {
				pWorld->SetTurnSpeed(0);
			}

			if (g_KeyDownUp) {
				pWorld->SetMoveSpeed(+1);
			}
			else if (g_KeyDownDown) {
				pWorld->SetMoveSpeed(-1);
			}
			else {
				pWorld->SetMoveSpeed(0);
			}

			// If none of the movement buttons are pressed, but the mouse
			// button is pressed, check to see if this should move the player.
			if (!g_KeyDownLeft && !g_KeyDownRight && !g_KeyDownUp && !g_KeyDownDown && g_LeftMouse) {
				pWorld->CheckMouseMovement();
			}

			pWorld->AdvanceTime(timeDelta);

			if (g_EnableFullScreen && g_EnableDirectDraw) {
				BYTE *pBuffer = NULL;
				DWORD pitch   = 0;
#if 0
				pWorld->Render();
				if (wrapper.LockRenderBuffer(&pBuffer, pitch)) {
					DWORD wordCount = g_WindowWidth >> 2;
					DWORD *pSrc = reinterpret_cast<DWORD*>(raster.GetFrameBuffer());
					DWORD *pDst = reinterpret_cast<DWORD*>(pBuffer);
					for (DWORD y = 0; y < g_WindowHeight; ++y) {
						for (DWORD x = 0; x < wordCount; ++x) {
							pDst[x] = pSrc[x];
						}
						pSrc += g_WindowWidth >> 2;
						pDst += pitch >> 2;
					}
					wrapper.UnlockRenderBuffer();
					raster.SetFrameBuffer(NULL, 0);
					wrapper.SetPalette(raster.GetPalette());
					wrapper.ShowRenderBuffer();
				}
#else
				if (wrapper.LockRenderBuffer(&pBuffer, pitch)) {
					raster.SetFrameBuffer(pBuffer, pitch);
					pWorld->Render();
					wrapper.UnlockRenderBuffer();
					raster.SetFrameBuffer(NULL, 0);
					wrapper.SetPalette(raster.GetPalette());
					wrapper.ShowRenderBuffer();
				}
#endif
			}
			else {
				pWorld->Render();
				HDC hDC = GetDC(g_hWindow);
				g_pRaster->DisplayFrame(hDC);
				ReleaseDC(g_hWindow, hDC);
			}

			prevTime = curTime;

			++frameCount;

			if ((curTime - fpsMarker) > 1000) {
				pWorld->SetFPS(frameCount);
				fpsMarker  = curTime;
				frameCount = 0;
			}

			Sleep(0);
		}
	} while (WM_QUIT != message.message);

	timeEndPeriod(timeCaps.wPeriodMin);

	g_pDDWrapper = NULL;
	g_pRaster = NULL;
	g_pWorld  = NULL;

	pWorld->Unload();

	SafeDelete(pWorld);

#ifdef ENABLE_PROFILER
	profileManager.SaveReport("profile.txt");
#endif

	return int(message.wParam);
}


