/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/DirectDrawWrapper.cpp 2     12/31/06 10:47a Lee $
//
//	File: DirectDrawWrapper.cpp
//
//
/////////////////////////////////////////////////////////////////////////////


#define INITGUID
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "DirectDrawWrapper.h"

#define ArraySize(x)	(sizeof(x) / sizeof((x)[0]))
#define SafeRelease(x)	{ if (NULL != (x)) { (x)->Release(); (x) = NULL; } }


struct DirectDrawError_t
{
	HRESULT HR;
	char*   Message;
};


DirectDrawError_t g_DirectDrawErrorTable[] =
{
	{ DD_OK,							"DD_OK: The request completed successfully." },
	{ DDERR_ALREADYINITIALIZED,			"DDERR_ALREADYINITIALIZED: The object has already been initialized." },
	{ DDERR_BLTFASTCANTCLIP,			"DDERR_BLTFASTCANTCLIP: A DirectDrawClipper object is attached to a source surface that has passed into a call to the IDirectDrawSurface5::BltFast method." },
	{ DDERR_CANNOTATTACHSURFACE,		"DDERR_CANNOTATTACHSURFACE: A surface cannot be attached to another requested surface." },
	{ DDERR_CANNOTDETACHSURFACE,		"DDERR_CANNOTDETACHSURFACE: A surface cannot be detached from another requested surface." },
	{ DDERR_CANTCREATEDC,				"DDERR_CANTCREATEDC: Windows cannot create any more device contexts (DCs), or a DC was requested for a palette-indexed surface when the surface had no palette and the display mode was not palette-indexed (that is, DirectDraw cannot select a proper palette into the DC)." },
	{ DDERR_CANTDUPLICATE,				"DDERR_CANTDUPLICATE: Primary and 3-D surfaces, or surfaces that are implicitly created, cannot be duplicated." },
	{ DDERR_CANTLOCKSURFACE,			"DDERR_CANTLOCKSURFACE: Access to this surface is refused because an attempt was made to lock the primary surface without DCI support." },
	{ DDERR_CANTPAGELOCK,				"DDERR_CANTPAGELOCK: An attempt to page lock a surface failed. Page lock will not work on a display-memory surface or an emulated primary surface." },
	{ DDERR_CANTPAGEUNLOCK,				"DDERR_CANTPAGEUNLOCK: An attempt to page unlock a surface failed. Page unlock will not work on a display-memory surface or an emulated primary surface." },
	{ DDERR_CLIPPERISUSINGHWND,			"DDERR_CLIPPERISUSINGHWND: An attempt was made to set a clip list for a DirectDrawClipper object that is already monitoring a window handle." },
	{ DDERR_COLORKEYNOTSET,				"DDERR_COLORKEYNOTSET: No source color key is specified for this operation." },
	{ DDERR_CURRENTLYNOTAVAIL,			"DDERR_CURRENTLYNOTAVAIL: No support is currently available." },
	{ DDERR_DCALREADYCREATED,			"DDERR_DCALREADYCREATED: A device context (DC) has already been returned for this surface. Only one DC can be retrieved for each surface." },
	{ DDERR_DEVICEDOESNTOWNSURFACE,		"DDERR_DEVICEDOESNTOWNSURFACE: Surfaces created by one DirectDraw device cannot be used directly by another DirectDraw device." },
	{ DDERR_DIRECTDRAWALREADYCREATED,	"DDERR_DIRECTDRAWALREADYCREATED: A DirectDraw object representing this driver has already been created for this process." },
	{ DDERR_EXCEPTION,					"DDERR_EXCEPTION: An exception was encountered while performing the requested operation." },
	{ DDERR_EXCLUSIVEMODEALREADYSET,	"DDERR_EXCLUSIVEMODEALREADYSET: An attempt was made to set the cooperative level when it was already set to exclusive." },
	{ DDERR_EXPIRED,					"DDERR_EXPIRED: The data has expired and is no longer valid." },
	{ DDERR_GENERIC,					"DDERR_GENERIC: There is an undefined error condition." },
	{ DDERR_HEIGHTALIGN,				"DDERR_HEIGHTALIGN: The height of the provided rectangle is not a multiple of the required alignment." },
	{ DDERR_HWNDALREADYSET,				"DDERR_HWNDALREADYSET: The DirectDraw cooperative level window handle has already been set. It cannot be reset while the process has surfaces or palettes created." },
	{ DDERR_HWNDSUBCLASSED,				"DDERR_HWNDSUBCLASSED: DirectDraw is prevented from restoring the state because the DirectDraw cooperative level window handle has been subclassed." },
	{ DDERR_IMPLICITLYCREATED,			"DDERR_IMPLICITLYCREATED: The surface cannot be restored because it is an implicitly created surface." },
	{ DDERR_INCOMPATIBLEPRIMARY,		"DDERR_INCOMPATIBLEPRIMARY: The primary surface creation request does not match the existing primary surface." },
	{ DDERR_INVALIDCAPS,				"DDERR_INVALIDCAPS: One or more of the capability bits passed to the callback function are incorrect." },
	{ DDERR_INVALIDCLIPLIST,			"DDERR_INVALIDCLIPLIST: DirectDraw does not support the provided clip list." },
	{ DDERR_INVALIDDIRECTDRAWGUID,		"DDERR_INVALIDDIRECTDRAWGUID: The globally unique identifier (GUID) passed to the DirectDrawCreate function is not a valid DirectDraw driver identifier." },
	{ DDERR_INVALIDMODE,				"DDERR_INVALIDMODE: DirectDraw does not support the requested mode." },
	{ DDERR_INVALIDOBJECT,				"DDERR_INVALIDOBJECT: DirectDraw received a pointer that was an invalid DirectDraw object." },
	{ DDERR_INVALIDPARAMS,				"DDERR_INVALIDPARAMS: One or more of the parameters passed to the method are incorrect." },
	{ DDERR_INVALIDPIXELFORMAT,			"DDERR_INVALIDPIXELFORMAT: The pixel format was invalid as specified." },
	{ DDERR_INVALIDPOSITION,			"DDERR_INVALIDPOSITION: The position of the overlay on the destination is no longer valid." },
	{ DDERR_INVALIDRECT,				"DDERR_INVALIDRECT: The provided rectangle was invalid." },
	{ DDERR_INVALIDSTREAM,				"DDERR_INVALIDSTREAM: The specified stream contains invalid data." },
	{ DDERR_INVALIDSURFACETYPE,			"DDERR_INVALIDSURFACETYPE: The requested operation could not be performed because the surface was of the wrong type." },
	{ DDERR_LOCKEDSURFACES,				"DDERR_LOCKEDSURFACES: One or more surfaces are locked, causing the failure of the requested operation." },
	{ DDERR_MOREDATA,					"DDERR_MOREDATA: There is more data available than the specified buffer size can hold." },
	{ DDERR_NO3D,						"DDERR_NO3D: No 3-D hardware or emulation is present." },
	{ DDERR_NOALPHAHW,					"DDERR_NOALPHAHW: No alpha acceleration hardware is present or available, causing the failure of the requested operation." },
	{ DDERR_NOBLTHW,					"DDERR_NOBLTHW: No blitter hardware is present." },
	{ DDERR_NOCLIPLIST,					"DDERR_NOCLIPLIST: No clip list is available." },
	{ DDERR_NOCLIPPERATTACHED,			"DDERR_NOCLIPPERATTACHED: No DirectDrawClipper object is attached to the surface object." },
	{ DDERR_NOCOLORCONVHW,				"DDERR_NOCOLORCONVHW: The operation cannot be carried out because no color-conversion hardware is present or available." },
	{ DDERR_NOCOLORKEY,					"DDERR_NOCOLORKEY: The surface does not currently have a color key." },
	{ DDERR_NOCOLORKEYHW,				"DDERR_NOCOLORKEYHW: The operation cannot be carried out because there is no hardware support for the destination color key." },
	{ DDERR_NOCOOPERATIVELEVELSET,		"DDERR_NOCOOPERATIVELEVELSET: A create function is called without the IDirectDraw4::SetCooperativeLevel method being called." },
	{ DDERR_NODC,						"DDERR_NODC: No DC has been created for this surface." },
	{ DDERR_NODDROPSHW,					"DDERR_NODDROPSHW: No DirectDraw raster operation (ROP) hardware is available." },
	{ DDERR_NODIRECTDRAWHW,				"DDERR_NODIRECTDRAWHW: Hardware-only DirectDraw object creation is not possible; the driver does not support any hardware." },
	{ DDERR_NODIRECTDRAWSUPPORT,		"DDERR_NODIRECTDRAWSUPPORT: DirectDraw support is not possible with the current display driver." },
	{ DDERR_NOEMULATION,				"DDERR_NOEMULATION: Software emulation is not available." },
	{ DDERR_NOEXCLUSIVEMODE,			"DDERR_NOEXCLUSIVEMODE: The operation requires the application to have exclusive mode, but the application does not have exclusive mode." },
	{ DDERR_NOFLIPHW,					"DDERR_NOFLIPHW: Flipping visible surfaces is not supported." },
	{ DDERR_NOFOCUSWINDOW,				"DDERR_NOFOCUSWINDOW: An attempt was made to create or set a device window without first setting the focus window." },
	{ DDERR_NOGDI,						"DDERR_NOGDI: No GDI is present." },
	{ DDERR_NOHWND,						"DDERR_NOHWND: Clipper notification requires a window handle, or no window handle has been previously set as the cooperative level window handle." },
	{ DDERR_NOMIPMAPHW,					"DDERR_NOMIPMAPHW: The operation cannot be carried out because no mipmap capable texture mapping hardware is present or available." },
	{ DDERR_NOMIRRORHW,					"DDERR_NOMIRRORHW: The operation cannot be carried out because no mirroring hardware is present or available." },
	{ DDERR_NONONLOCALVIDMEM,			"DDERR_NONONLOCALVIDMEM: An attempt was made to allocate non-local video memory from a device that does not support non-local video memory." },
	{ DDERR_NOOPTIMIZEHW,				"DDERR_NOOPTIMIZEHW: The device does not support optimized surfaces." },
	{ DDERR_NOOVERLAYDEST,				"DDERR_NOOVERLAYDEST: The IDirectDrawSurface5::GetOverlayPosition method is called on an overlay but the IDirectDrawSurface5::UpdateOverlay method has not been called on to establish a destination." },
	{ DDERR_NOOVERLAYHW,				"DDERR_NOOVERLAYHW: The operation cannot be carried out because no overlay hardware is present or available." },
	{ DDERR_NOPALETTEATTACHED,			"DDERR_NOPALETTEATTACHED: No palette object is attached to this surface." },
	{ DDERR_NOPALETTEHW,				"DDERR_NOPALETTEHW: There is no hardware support for 16- or 256-color palettes." },
	{ DDERR_NORASTEROPHW,				"DDERR_NORASTEROPHW: The operation cannot be carried out because no appropriate raster operation hardware is present or available." },
	{ DDERR_NOROTATIONHW,				"DDERR_NOROTATIONHW: The operation cannot be carried out because no rotation hardware is present or available." },
	{ DDERR_NOSTRETCHHW,				"DDERR_NOSTRETCHHW: The operation cannot be carried out because there is no hardware support for stretching." },
	{ DDERR_NOT4BITCOLOR,				"DDERR_NOT4BITCOLOR: The DirectDrawSurface object is not using a 4-bit color palette and the requested operation requires a 4-bit color palette." },
	{ DDERR_NOT4BITCOLORINDEX,			"DDERR_NOT4BITCOLORINDEX: The DirectDrawSurface object is not using a 4-bit color index palette and the requested operation requires a 4-bit color index palette." },
	{ DDERR_NOT8BITCOLOR,				"DDERR_NOT8BITCOLOR: The DirectDrawSurface object is not using an 8-bit color palette and the requested operation requires an 8-bit color palette." },
	{ DDERR_NOTAOVERLAYSURFACE,			"DDERR_NOTAOVERLAYSURFACE: An overlay component is called for a non-overlay surface." },
	{ DDERR_NOTEXTUREHW,				"DDERR_NOTEXTUREHW: The operation cannot be carried out because no texture-mapping hardware is present or available." },
	{ DDERR_NOTFLIPPABLE,				"DDERR_NOTFLIPPABLE: An attempt has been made to flip a surface that cannot be flipped." },
	{ DDERR_NOTFOUND,					"DDERR_NOTFOUND: The requested item was not found." },
	{ DDERR_NOTINITIALIZED,				"DDERR_NOTINITIALIZED: An attempt was made to call an interface method of a DirectDraw object before the object was initialized." },
	{ DDERR_NOTLOADED,					"DDERR_NOTLOADED: The surface is an optimized surface, but it has not yet been allocated any memory." },
	{ DDERR_NOTLOCKED,					"DDERR_NOTLOCKED: An attempt is made to unlock a surface that was not locked." },
	{ DDERR_NOTPAGELOCKED,				"DDERR_NOTPAGELOCKED: An attempt is made to page unlock a surface with no outstanding page locks." },
	{ DDERR_NOTPALETTIZED,				"DDERR_NOTPALETTIZED: The surface being used is not a palette-based surface." },
	{ DDERR_NOVSYNCHW,					"DDERR_NOVSYNCHW: The operation cannot be carried out because there is no hardware support for vertical blank synchronized operations." },
	{ DDERR_NOZBUFFERHW,				"DDERR_NOZBUFFERHW: The operation to create a Z-buffer in display memory or to perform a blit using a Z-buffer cannot be carried out because there is no hardware support for Z-buffers." },
	{ DDERR_NOZOVERLAYHW,				"DDERR_NOZOVERLAYHW: The overlay surfaces cannot be Z-layered based on the Z-order because the hardware does not support Z-ordering of overlays." },
	{ DDERR_OUTOFCAPS,					"DDERR_OUTOFCAPS: The hardware needed for the requested operation has already been allocated." },
	{ DDERR_OUTOFMEMORY,				"DDERR_OUTOFMEMORY: DirectDraw does not have enough memory to perform the operation." },
	{ DDERR_OUTOFVIDEOMEMORY,			"DDERR_OUTOFVIDEOMEMORY: DirectDraw does not have enough display memory to perform the operation." },
	{ DDERR_OVERLAPPINGRECTS,			"DDERR_OVERLAPPINGRECTS: Operation could not be carried out because the source and destination rectangles are on the same surface and overlap each other." },
	{ DDERR_OVERLAYCANTCLIP,			"DDERR_OVERLAYCANTCLIP: The hardware does not support clipped overlays." },
	{ DDERR_OVERLAYCOLORKEYONLYONEACTIVE, "DDERR_OVERLAYCOLORKEYONLYONEACTIVE: An attempt was made to have more than one color key active on an overlay." },
	{ DDERR_OVERLAYNOTVISIBLE,			"DDERR_OVERLAYNOTVISIBLE: The IDirectDrawSurface5::GetOverlayPosition method is called on a hidden overlay." },
	{ DDERR_PALETTEBUSY,				"DDERR_PALETTEBUSY: Access to this palette is refused because the palette is locked by another thread." },
	{ DDERR_PRIMARYSURFACEALREADYEXISTS, "DDERR_PRIMARYSURFACEALREADYEXISTS: This process has already created a primary surface." },
	{ DDERR_REGIONTOOSMALL,				"DDERR_REGIONTOOSMALL: The region passed to the IDirectDrawClipper::GetClipList method is too small." },
	{ DDERR_SURFACEALREADYATTACHED,		"DDERR_SURFACEALREADYATTACHED: An attempt was made to attach a surface to another surface while they are already attached." },
	{ DDERR_SURFACEALREADYDEPENDENT,	"DDERR_SURFACEALREADYDEPENDENT: An attempt was made to make a surface a dependency of another surface to which it is already dependent." },
	{ DDERR_SURFACEBUSY,				"DDERR_SURFACEBUSY: Access to the surface is refused because the surface is locked by another thread." },
	{ DDERR_SURFACEISOBSCURED,			"DDERR_SURFACEISOBSCURED: Access to the surface is refused because the surface is obscured." },
	{ DDERR_SURFACELOST,				"DDERR_SURFACELOST: Access to the surface is refused because the surface memory is gone. Call the IDirectDrawSurface5::Restore method on this surface to restore the memory associated with it." },
	{ DDERR_SURFACENOTATTACHED,			"DDERR_SURFACENOTATTACHED: The requested surface is not attached." },
	{ DDERR_TOOBIGHEIGHT,				"DDERR_TOOBIGHEIGHT: The height requested by DirectDraw is too large." },
	{ DDERR_TOOBIGSIZE,					"DDERR_TOOBIGSIZE: The size requested by DirectDraw is too large. However, the individual height and width are valid sizes." },
	{ DDERR_TOOBIGWIDTH,				"DDERR_TOOBIGWIDTH: The width requested by DirectDraw is too large." },
	{ DDERR_UNSUPPORTED,				"DDERR_UNSUPPORTED: The operation is not supported." },
	{ DDERR_UNSUPPORTEDFORMAT,			"DDERR_UNSUPPORTEDFORMAT: The pixel format requested is not supported by DirectDraw." },
	{ DDERR_UNSUPPORTEDMASK,			"DDERR_UNSUPPORTEDMASK: The bitmask in the pixel format requested is not supported by DirectDraw." },
	{ DDERR_UNSUPPORTEDMODE,			"DDERR_UNSUPPORTEDMODE: The display is currently in an unsupported mode." },
	{ DDERR_VERTICALBLANKINPROGRESS,	"DDERR_VERTICALBLANKINPROGRESS: A vertical blank is in progress." },
	{ DDERR_VIDEONOTACTIVE,				"DDERR_VIDEONOTACTIVE: The video port is not active." },
	{ DDERR_WASSTILLDRAWING,			"DDERR_WASSTILLDRAWING: The previous blit operation that is transferring information to or from this surface is incomplete." },
	{ DDERR_WRONGMODE,					"DDERR_WRONGMODE: This surface cannot be restored because it was created in a different mode." },
	{ DDERR_XALIGN,						"DDERR_XALIGN: The provided rectangle was not horizontally aligned on a required boundary." }
};


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CDirectDrawWrapper::CDirectDrawWrapper(void)
	:	m_pDraw(NULL),
		m_pClipper(NULL),
		m_pPrimary(NULL),
		m_pBackBuffer(NULL),
		m_RenderIndex(0),
		m_pPalette(NULL)
{
	m_ErrorMessage[0] = '\0';

	m_pRenderBuffer[0] = NULL;
	m_pRenderBuffer[1] = NULL;

	m_BlitCount = 0;
	m_BlitTime  = 0.0f;
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CDirectDrawWrapper::~CDirectDrawWrapper(void)
{
	SafeRelease(m_pPalette);
	SafeRelease(m_pRenderBuffer[1]);
	SafeRelease(m_pRenderBuffer[0]);
	SafeRelease(m_pBackBuffer);
	SafeRelease(m_pPrimary);
	SafeRelease(m_pClipper);
	SafeRelease(m_pDraw);
}


/////////////////////////////////////////////////////////////////////////////
//
//	WriteErrorMessage()
//
void CDirectDrawWrapper::WriteErrorMessage(char who[], HRESULT hr)
{
	long errorIndex = -1;
	for (long i = 0; i < ArraySize(g_DirectDrawErrorTable); ++i) {
		if (g_DirectDrawErrorTable[i].HR == hr) {
			errorIndex = i;
			break;
		}
	}

	if (errorIndex != -1) {
		_snprintf(m_ErrorMessage, ArraySize(m_ErrorMessage), "%s, hr = 0x%08X, %s\n", who, hr, g_DirectDrawErrorTable[errorIndex].Message);
	}
	else {
		_snprintf(m_ErrorMessage, ArraySize(m_ErrorMessage), "%s, hr = 0x%08X, unknown error code\n", who, hr);
	}

	OutputDebugString(m_ErrorMessage);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Initialize()
//
bool CDirectDrawWrapper::Initialize(HWND hWindow, DWORD width, DWORD height)
{
	m_ScreenWidth  = width;
	m_ScreenHeight = height;
	m_RenderWidth  = (width == 640) ? 640 : 960;
	m_RenderHeight = (width == 640) ? 400 : 600;

	//
	// Create the main DirectDraw interface.
	//

	IDirectDraw *pDraw = NULL;
	HRESULT hr;

	hr = DirectDrawCreate(NULL, &pDraw, NULL);

	if (FAILED(hr)) {
		WriteErrorMessage("DirectDrawCreate() failed", hr);
		return false;
	}

	hr = pDraw->QueryInterface(IID_IDirectDraw7, (void**)&m_pDraw);
	SafeRelease(pDraw);

	if (FAILED(hr)) {
		_snprintf(m_ErrorMessage, ArraySize(m_ErrorMessage), "QueryInterface(IDirectDraw7) failed, hr = 0x%08X", hr);
		return false;
	}


//	hr = m_pDraw->SetCooperativeLevel(hWindow, DDSCL_NORMAL);
	hr = m_pDraw->SetCooperativeLevel(hWindow, DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX | DDSCL_EXCLUSIVE);
	if (FAILED(hr)) {
		WriteErrorMessage("SetCooperativeLevel() failed", hr);
		return false;
	}

	m_pDraw->SetDisplayMode(width, height, 8, 0, 0);

	PALETTEENTRY palette[256];
	hr = m_pDraw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_INITIALIZE, palette, &m_pPalette, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreatePalette() failed", hr);
		return false;
	}



	//
	// Create clipper.
	//
/*
	hr = m_pDraw->CreateClipper(0, &m_pClipper, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateClipper() failed", hr);
		return false;
	}

	hr = m_pClipper->SetHWnd(0, hWindow);
	if (FAILED(hr)) {
		WriteErrorMessage("Clipper->SetHWnd() failed", hr);
		return false;
	}
*/
	m_ClearCount = 0;

	DDSURFACEDESC2 desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc); 
	desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT; 
	desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	desc.dwBackBufferCount = 2;

	hr = m_pDraw->CreateSurface(&desc, &m_pPrimary, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface() failed for primary surface", hr);
		return false;
	}
/*
	DDSCAPS2 caps;
	memset(&caps, 0, sizeof(caps));
	caps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = m_pPrimary->GetAttachedSurface(&caps, &m_pBackBuffer);
	if (FAILED(hr)) {
		WriteErrorMessage("GetAttachedSurface() failed", hr);
		return false;
	}
*/
	hr = m_pPrimary->SetPalette(m_pPalette);
	if (FAILED(hr)) {
		WriteErrorMessage("SetPalette() failed", hr);
		return false;
	}

	ClearSurface(m_pPrimary);
//	m_pBackBuffer->Blt(&rect, NULL, NULL,  DDBLT_COLORFILL | DDBLT_WAIT, &fx);


//   if (FAILED(lpddsprimary->GetAttachedSurface(&ddscaps,&lpddsback)))

/*
	hr = m_pPrimary->SetClipper(m_pClipper);
	if (FAILED(hr)) {
		WriteErrorMessage("SetClipper() failed for primary surface", hr);
		return false;
	}
*/
/*
	memset(&desc, 0, sizeof(desc));
	desc.dwSize			 = sizeof(desc);
	desc.ddsCaps.dwCaps  = DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN;
	desc.dwFlags		 = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
	desc.dwWidth		 = m_RenderWidth;
	desc.dwHeight		 = m_RenderHeight;
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_RGB | DDPF_PALETTEINDEXED8;
	desc.ddpfPixelFormat.dwRGBBitCount	= 8;

	hr = m_pDraw->CreateSurface(&desc, &m_pBackBuffer, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface() failed for back buffer", hr);
		return false;
	}

	m_LockedRenderBuffer = false;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize			 = sizeof(desc);
	desc.ddsCaps.dwCaps  = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
	desc.dwFlags		 = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
	desc.dwWidth		 = m_RenderWidth;
	desc.dwHeight		 = m_RenderHeight;
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_RGB | DDPF_PALETTEINDEXED8;
	desc.ddpfPixelFormat.dwRGBBitCount	= 8;

	hr = m_pDraw->CreateSurface(&desc, &m_pRenderBuffer[0], NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface() failed for render buffer", hr);
		return false;
	}

	hr = m_pDraw->CreateSurface(&desc, &m_pRenderBuffer[1], NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface() failed for render buffer", hr);
		return false;
	}
*/
	m_LockedRenderBuffer = false;

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
void Format8bit(DDSURFACEDESC2 &desc)
{
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_RGB | DDPF_PALETTEINDEXED8;
	desc.ddpfPixelFormat.dwRGBBitCount	= 8;
}


/////////////////////////////////////////////////////////////////////////////
//
void Format422(DDSURFACEDESC2 &desc)
{
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_FOURCC;
	desc.ddpfPixelFormat.dwFourCC		= 0x59565955;
//	desc.ddpfPixelFormat.dwFourCC		= 0x32595559;
//	desc.ddpfPixelFormat.dwFourCC		= 0x32315659;
	desc.ddpfPixelFormat.dwYUVBitCount	= 16;
	desc.ddpfPixelFormat.dwYBitMask		= 0xFF00FF00;
	desc.ddpfPixelFormat.dwVBitMask		= 0x00FF0000;
	desc.ddpfPixelFormat.dwUBitMask		= 0x000000FF;
}


/////////////////////////////////////////////////////////////////////////////
//
void FormatRGB16(DDSURFACEDESC2 &desc)
{
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_RGB;
	desc.ddpfPixelFormat.dwRGBBitCount	= 16;
	desc.ddpfPixelFormat.dwRBitMask		= 0x0000F800;
	desc.ddpfPixelFormat.dwGBitMask		= 0x000007E0;
	desc.ddpfPixelFormat.dwBBitMask		= 0x0000001F;
//	desc.ddpfPixelFormat.dwRBitMask		= 0x00007C00;
//	desc.ddpfPixelFormat.dwGBitMask		= 0x000003E0;
//	desc.ddpfPixelFormat.dwBBitMask		= 0x0000001F;
}


/////////////////////////////////////////////////////////////////////////////
//
void FormatRGB32(DDSURFACEDESC2 &desc)
{
	desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags		= DDPF_RGB;
	desc.ddpfPixelFormat.dwRGBBitCount	= 32;
	desc.ddpfPixelFormat.dwRBitMask		= 0x00FF0000;
	desc.ddpfPixelFormat.dwGBitMask		= 0x0000FF00;
	desc.ddpfPixelFormat.dwBBitMask		= 0x000000FF;
}


/////////////////////////////////////////////////////////////////////////////
//
void CDirectDrawWrapper::ClearSurface(IDirectDrawSurface7 *pSurface)
{
	DDBLTFX fx;
	memset(&fx, 0, sizeof(fx));
	fx.dwSize = sizeof(fx);
	fx.dwFillColor = 0;

	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = m_ScreenWidth;
	rect.bottom = m_ScreenHeight;

	pSurface->Blt(&rect, NULL, NULL,  DDBLT_COLORFILL | DDBLT_WAIT, &fx);
}


/////////////////////////////////////////////////////////////////////////////
//
bool CDirectDrawWrapper::LockRenderBuffer(BYTE **ppMemory, DWORD &pitch)
{
	HRESULT hr;

	if ((false == m_LockedRenderBuffer) && (NULL != m_pPrimary)) {
		SafeRelease(m_pBackBuffer);

		DDSCAPS2 caps;
		memset(&caps, 0, sizeof(caps));
		caps.dwCaps = DDSCAPS_BACKBUFFER;
		hr = m_pPrimary->GetAttachedSurface(&caps, &m_pBackBuffer);
		if (FAILED(hr)) {
			WriteErrorMessage("GetAttachedSurface() failed", hr);
			return false;
		}

		if (m_ClearCount++ < 3) {
			ClearSurface(m_pBackBuffer);
		}

		DDSURFACEDESC2 desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwSize			 = sizeof(desc);
		desc.ddsCaps.dwCaps  = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
		desc.dwFlags		 = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
		desc.dwWidth		 = m_RenderWidth;
		desc.dwHeight		 = m_RenderHeight;
		desc.ddpfPixelFormat.dwSize			= sizeof(DDPIXELFORMAT);
		desc.ddpfPixelFormat.dwFlags		= DDPF_RGB | DDPF_PALETTEINDEXED8;
		desc.ddpfPixelFormat.dwRGBBitCount	= 8;

		m_RenderIndex = m_RenderIndex ? 0 : 1;

//		hr = m_pRenderBuffer[m_RenderIndex]->Lock(NULL, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY/* | DDLOCK_NOSYSLOCK*/, NULL);
		hr = m_pBackBuffer->Lock(NULL, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY/* | DDLOCK_NOSYSLOCK*/, NULL);

		if (SUCCEEDED(hr)) {
			*ppMemory	= reinterpret_cast<BYTE*>(desc.lpSurface)
						+ (desc.lPitch * ((m_ScreenHeight - m_RenderHeight) / 2))
						+ ((m_ScreenWidth - m_RenderWidth) / 2);

			pitch = desc.lPitch;
			m_LockedRenderBuffer = true;
			return true;
		}

		WriteErrorMessage("Lock() failed for render buffer", hr);
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////
//
void CDirectDrawWrapper::UnlockRenderBuffer(void)
{
	if (m_LockedRenderBuffer) {
		m_LockedRenderBuffer = false;
//		m_pRenderBuffer[m_RenderIndex]->Unlock(NULL);
		m_pBackBuffer->Unlock(NULL);
		SafeRelease(m_pBackBuffer);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
void CDirectDrawWrapper::ShowRenderBuffer(void)
{
	RECT srcRect, dstRect;
	srcRect.left   = 0;
	srcRect.top    = 0;
	srcRect.right  = m_RenderWidth;
	srcRect.bottom = m_RenderHeight;

	dstRect.left   = (m_ScreenWidth  - m_RenderWidth)  / 2;
	dstRect.top    = (m_ScreenHeight - m_RenderHeight) / 2;
	dstRect.right  = dstRect.left + m_RenderWidth;
	dstRect.bottom = dstRect.top  + m_RenderHeight;

	m_pPrimary->Flip(NULL, DDFLIP_WAIT);

//	m_pPrimary->SetPalette(m_pPalette);
/*
	HRESULT hr = m_pBackBuffer->Blt(&srcRect, m_pRenderBuffer[m_RenderIndex], &srcRect, DDBLT_WAIT, NULL);
//DDBLT_WAIT
	if (SUCCEEDED(hr)) {
		hr = m_pPrimary->Blt(&dstRect, m_pBackBuffer, &srcRect, DDBLT_WAIT, NULL);

		if (FAILED(hr)) {
			OutputDebugString("primary failed\n");
			WriteErrorMessage("Blt() failed for primary buffer", hr);
		}
	}
	else {
		OutputDebugString("back failed\n");
		WriteErrorMessage("Blt() failed for back buffer", hr);
	}
*/
}


/////////////////////////////////////////////////////////////////////////////
//
void CDirectDrawWrapper::SetPalette(DWORD *pPalette)
{
	if (NULL != m_pPalette) {
		DWORD palette[256];
		for (DWORD i = 0; i < 256; ++i) {
			register DWORD value = pPalette[i];
			palette[i] = (value & 0xFF00FF00) | ((value & 0x000000FF) << 16) | ((value & 0x00FF0000) >> 16);
		}
		HRESULT hr = m_pPalette->SetEntries(0, 0, 256, reinterpret_cast<PALETTEENTRY*>(palette));
		if (FAILED(hr)) {
			OutputDebugString("SetPalette failed\n");
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
void CDirectDrawWrapper::TestBlit(BYTE *pBuffer, long x, long y)
{
	DDSURFACEDESC2 desc;
	IDirectDrawSurface7 *pSurface = NULL;
	IDirectDrawSurface7 *pVidSurface = NULL;


	// The documentation has instructions for creating a surface from an
	// existing buffer in system memory, but those instructions are wrong
	// (or possibly have changed since version 5 of DirectDraw).
	//
	// Whatever the reason, setting the DDSD_LPSURFACE flag will cause an
	// error when attempting to create the surface.  Instead, the flags
	// DDSCAPS_LOCALVIDMEM and DDSCAPS_VIDEOMEMORY need to be set in the
	// dwCaps field.  This was derived via experimentation (looking at
	// the description struct returned from Lock), so the why-for of this
	// is unknown.  The lpSurface field should still be set to point to
	// the system memory buffer, and the buffer created will be initialized
	// with the given image data.

	// Initialize the surface description.
	memset(&desc, 0, sizeof(desc));
	desc.dwSize			 = sizeof(desc);
//	desc.ddsCaps.dwCaps  = DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
	desc.ddsCaps.dwCaps  = DDSCAPS_SYSTEMMEMORY;
	desc.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
	desc.dwFlags		 = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
//	desc.dwFlags		|= DDSD_LPSURFACE;
//	desc.dwFlags		|= DDSD_PITCH;
	desc.dwWidth		 = 320;
	desc.dwHeight		 = 240;
//	desc.lPitch			 = 320;
//	desc.lpSurface		 = pBuffer;

	Format8bit(desc);
//	Format422(desc);
//	FormatRGB16(desc);
//	FormatRGB32(desc);


	// Create the surface
	HRESULT hr = m_pDraw->CreateSurface(&desc, &pSurface, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface(system) failed", hr);
		return;
	}

	desc.ddsCaps.dwCaps  = DDSCAPS_VIDEOMEMORY;
	desc.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
//	Format422(desc);
	Format8bit(desc);

		__int64 start, stop, freq;
		QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		QueryPerformanceCounter((LARGE_INTEGER*)&start);

	hr = m_pDraw->CreateSurface(&desc, &pVidSurface, NULL);
	if (FAILED(hr)) {
		WriteErrorMessage("CreateSurface(video) failed", hr);
		pSurface->Release();
		return;
	}
	else if ((NULL != pSurface) && (NULL != m_pPrimary) && (NULL != pVidSurface)) {
/*
desc.ddpfPixelFormat.dwFlags		= DDPF_FOURCC;
desc.ddpfPixelFormat.dwFourCC		= 0x59565955;
desc.ddpfPixelFormat.dwYUVBitCount	= 16;
desc.ddpfPixelFormat.dwYBitMask		= 0xFF00FF00;
desc.ddpfPixelFormat.dwVBitMask		= 0x00FF0000;
desc.ddpfPixelFormat.dwUBitMask		= 0x000000FF;

hr = pSurface->SetSurfaceDesc(&desc, 0);
if (FAILED(hr)) {
	WriteErrorMessage("SetSurfaceDesc() failed for test surface", hr);
}
*/

		pSurface->SetPalette(m_pPalette);
		pVidSurface->SetPalette(m_pPalette);
m_pPrimary->SetPalette(m_pPalette);

//		pSurface->Lock(NULL, &desc, DDLOCK_WRITEONLY, NULL);
		pVidSurface->Lock(NULL, &desc, DDLOCK_WRITEONLY, NULL);
		memcpy(desc.lpSurface, pBuffer, 320 * 240);
//		pSurface->Unlock(NULL);
		pVidSurface->Unlock(NULL);

		QueryPerformanceCounter((LARGE_INTEGER*)&stop);
		m_BlitCount += 1;
		m_BlitTime  += float(double(stop - start) / double(freq));

/*
		memset(&desc, 0, sizeof(desc));
		desc.dwSize			 = sizeof(desc);
		desc.dwFlags		 = DDSD_PIXELFORMAT | DDSD_CAPS;
	desc.ddsCaps.dwCaps  = DDSCAPS_VIDEOMEMORY;
	desc.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
	desc.dwFlags		 |= DDSD_WIDTH | DDSD_HEIGHT;
//	desc.dwFlags		 |= DDSD_PITCH;
	desc.dwWidth		 = 320;
	desc.dwHeight		 = 240;
//	desc.lPitch			 = 320 * 2;
//		Format422(desc);
		FormatRGB16(desc);
		pVidSurface->GetSurfaceDesc(&desc);
		hr = pVidSurface->SetSurfaceDesc(&desc, 0);
		if (FAILED(hr)) {
			WriteErrorMessage("SetSurfaceDesc() failed", hr);
		}
*/
		RECT srcRect = { 0, 0, 320, 240 };
		RECT dstRect = { 0, 0, 320, 240 };

		hr = pVidSurface->Blt(&dstRect, pSurface, &srcRect, DDBLT_WAIT, NULL);
		if (FAILED(hr)) {
			WriteErrorMessage("secondary->Blt() failed", hr);
		}
		else {
			dstRect.left   += x;
			dstRect.right  += x;
			dstRect.top    += y;
			dstRect.bottom += y;

			hr = m_pPrimary->Blt(&dstRect, pSurface, &srcRect, DDBLT_WAIT, NULL);
//			hr = m_pPrimary->Blt(&dstRect, pVidSurface, &srcRect, DDBLT_WAIT, NULL);
			if (FAILED(hr)) {
				WriteErrorMessage("primary->Blt() failed", hr);
			}
		}

		pSurface->Release();
		pVidSurface->Release();
	}
}



#define TestCaps(word,file,mask)	{ if (0 != ((mask)&(word))) { fprintf(file, "    " #mask "\n"); } }
#define Decimalize(x)				((x) / 1000),((x) % 1000)


DWORD AlphaBitDepth(DWORD mask)
{
	if (DDBD_1 == mask) {
		return 1;
	}

	if (DDBD_2 == mask) {
		return 2;
	}

	if (DDBD_4 == mask) {
		return 4;
	}

	if (DDBD_8 == mask) {
		return 8;
	}

	return 0;
}

/*
/////////////////////////////////////////////////////////////////////////////
//
//	LogCapabilities()
//
void CDirectDrawWrapper::LogCapabilities(DDCAPS &caps, FILE *pFile)
{
	fprintf(pFile, "\nCaps:\n");
	LogCaps(caps.dwCaps, pFile);

	fprintf(pFile, "\nCaps2:\n");
	LogCaps2(caps.dwCaps2, pFile);
	
	fprintf(pFile, "\nColor-Key Caps:\n");
	LogKeyCaps(caps.dwCKeyCaps, pFile);

	fprintf(pFile, "\nFx Caps:\n");
	LogFxCaps(caps.dwFXCaps, pFile);

	fprintf(pFile, "\nFx Alpha Caps:\n");
	LogAlphaCaps(caps.dwFXAlphaCaps, pFile);

	fprintf(pFile, "\nPalette Caps:\n");
	LogPaletteCaps(caps.dwPalCaps, pFile);

//	TestCaps(caps.dwSVCaps, pFile, DDSVCAPS_ENIGMA);
//	TestCaps(caps.dwSVCaps, pFile, DDSVCAPS_FLICKER);
//	TestCaps(caps.dwSVCaps, pFile, DDSVCAPS_REDBLUE);
//	TestCaps(caps.dwSVCaps, pFile, DDSVCAPS_SPLIT);



	fprintf(pFile, "\nSystem-Memory to Display-Memory Caps:\n");
	LogCaps(caps.dwSVBCaps, pFile);

	fprintf(pFile, "\nSystem-Memory to Display-Memory Color Key Caps:\n");
	LogKeyCaps(caps.dwSVBCKeyCaps, pFile);

	fprintf(pFile, "\nSystem-Memory to Display-Memory Fx Caps:\n");
	LogFxCaps(caps.dwSVBFXCaps, pFile);



	fprintf(pFile, "\nDisplay-Memory to System-Memory Caps:\n");
	LogCaps(caps.dwVSBCaps, pFile);

	fprintf(pFile, "\nDisplay-Memory to System-Memory Color Key Caps:\n");
	LogKeyCaps(caps.dwVSBCKeyCaps, pFile);

	fprintf(pFile, "\nDisplay-Memory to System-Memory Fx Caps:\n");
	LogFxCaps(caps.dwVSBFXCaps, pFile);



	fprintf(pFile, "\nSystem-Memory to System-Memory Caps:\n");
	LogCaps(caps.dwSSBCaps, pFile);

	fprintf(pFile, "\nSystem-Memory to System-Memory Color Key Caps:\n");
	LogKeyCaps(caps.dwSSBCKeyCaps, pFile);

	fprintf(pFile, "\nSystem-Memory to System-Memory Fx Caps:\n");
	LogFxCaps(caps.dwSSBFXCaps, pFile);



	fprintf(pFile, "\nNon-Local Memory Caps:\n");
	LogCaps(caps.dwNLVBCaps, pFile);

	fprintf(pFile, "\nNon-Local Memory Caps2:\n");
	LogCaps2(caps.dwNLVBCaps2, pFile);

	fprintf(pFile, "\nNon-Local Memory Color-Key Caps:\n");
	LogKeyCaps(caps.dwNLVBCKeyCaps, pFile);

	fprintf(pFile, "\nNon-Local Memory Fx Caps:\n");
	LogFxCaps(caps.dwNLVBFXCaps, pFile);



	fprintf(pFile, "\nMisc:\n");
	fprintf(pFile, "    dwAlphaBltConstBitDepths       = %d\n", AlphaBitDepth(caps.dwAlphaBltConstBitDepths));
	fprintf(pFile, "    dwAlphaBltPixelBitDepths       = %d\n", AlphaBitDepth(caps.dwAlphaBltPixelBitDepths));
	fprintf(pFile, "    dwAlphaBltSurfaceBitDepths     = %d\n", AlphaBitDepth(caps.dwAlphaBltSurfaceBitDepths));
	fprintf(pFile, "    dwAlphaOverlayConstBitDepths   = %d\n", AlphaBitDepth(caps.dwAlphaOverlayConstBitDepths));
	fprintf(pFile, "    dwAlphaOverlayPixelBitDepths   = %d\n", AlphaBitDepth(caps.dwAlphaOverlayPixelBitDepths));
	fprintf(pFile, "    dwAlphaOverlaySurfaceBitDepths = %d\n", AlphaBitDepth(caps.dwAlphaOverlaySurfaceBitDepths));
	fprintf(pFile, "    dwVidMemTotal         = %d KB\n", caps.dwVidMemTotal / 1024);
	fprintf(pFile, "    dwVidMemFree          = %d KB\n", caps.dwVidMemFree / 1024);
	fprintf(pFile, "    dwMaxVisibleOverlays  = %d\n", caps.dwMaxVisibleOverlays);
	fprintf(pFile, "    dwCurrVisibleOverlays = %d\n", caps.dwCurrVisibleOverlays);
	fprintf(pFile, "    dwNumFourCCCodes      = %d\n", caps.dwNumFourCCCodes);
	fprintf(pFile, "    dwAlignBoundarySrc    = %d\n", caps.dwAlignBoundarySrc);
	fprintf(pFile, "    dwAlignSizeSrc        = %d\n", caps.dwAlignSizeSrc);
	fprintf(pFile, "    dwAlignBoundaryDest   = %d\n", caps.dwAlignBoundaryDest);
	fprintf(pFile, "    dwAlignSizeDest       = %d\n", caps.dwAlignSizeDest);
	fprintf(pFile, "    dwAlignStrideAlign    = %d\n", caps.dwAlignStrideAlign);
	fprintf(pFile, "    dwMinOverlayStretch   = %d.%03d\n", Decimalize(caps.dwMinOverlayStretch));
	fprintf(pFile, "    dwMaxOverlayStretch   = %d.%03d\n", Decimalize(caps.dwMaxOverlayStretch));
	fprintf(pFile, "    dwMinLiveVideoStretch = %d.%03d\n", Decimalize(caps.dwMinLiveVideoStretch));
	fprintf(pFile, "    dwMaxLiveVideoStretch = %d.%03d\n", Decimalize(caps.dwMaxLiveVideoStretch));
	fprintf(pFile, "    dwMinHwCodecStretch   = %d.%03d\n", Decimalize(caps.dwMinHwCodecStretch));
	fprintf(pFile, "    dwMaxHwCodecStretch   = %d.%03d\n", Decimalize(caps.dwMaxHwCodecStretch));
	fprintf(pFile, "    dwAlignStrideAlign    = %d\n", caps.dwAlignStrideAlign);
	fprintf(pFile, "    dwMaxVideoPorts       = %d\n", caps.dwMaxVideoPorts);
	fprintf(pFile, "    dwCurrVideoPorts      = %d\n", caps.dwCurrVideoPorts);

	if ((caps.dwNumFourCCCodes > 0) && (caps.dwNumFourCCCodes < 128)) {
		fprintf(pFile, "\nFOURCC codes: %d\n", caps.dwNumFourCCCodes);
		DWORD count = caps.dwNumFourCCCodes;
		DWORD fourcc[128];

		m_pDraw->GetFourCCCodes(&count, fourcc);

		for (DWORD i = 0; i < count; ++i) {
			DWORD block[2];
			block[0] = fourcc[i];
			block[1] = 0;
			fprintf(pFile, "    0x%08X = %s\n", block[0], block);
		}
	}

	fprintf(pFile, "\nSurface Caps:\n");
//	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_3D);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_3DDEVICE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_ALLOCONLOAD);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_ALPHA);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_BACKBUFFER);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_COMPLEX);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_FLIP);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_FRONTBUFFER);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_HWCODEC);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_LIVEVIDEO);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_LOCALVIDMEM);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_MIPMAP);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_MODEX);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_NONLOCALVIDMEM);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_OFFSCREENPLAIN);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_OPTIMIZED);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_OVERLAY);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_OWNDC);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_PALETTE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_PRIMARYSURFACE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_PRIMARYSURFACELEFT);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_STANDARDVGAMODE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_SYSTEMMEMORY);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_TEXTURE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_VIDEOMEMORY);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_VIDEOPORT);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_VISIBLE);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_WRITEONLY);
	TestCaps(caps.ddsCaps.dwCaps, pFile, DDSCAPS_ZBUFFER);

	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_HARDWAREDEINTERLACE);
	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_HINTANTIALIASING);
	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_HINTDYNAMIC);
	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_HINTSTATIC);
	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_OPAQUE);
	TestCaps(caps.ddsCaps.dwCaps2, pFile, DDSCAPS2_TEXTUREMANAGE);

//	TestCaps(caps.ddsCaps.dwCaps4, pFile, DDSCAPS4_NONGDIPRIMARY);
}


void CDirectDrawWrapper::LogCaps(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDCAPS_3D);
	TestCaps(flags, pFile, DDCAPS_ALIGNBOUNDARYDEST);
	TestCaps(flags, pFile, DDCAPS_ALIGNBOUNDARYSRC);
	TestCaps(flags, pFile, DDCAPS_ALIGNSIZEDEST);
	TestCaps(flags, pFile, DDCAPS_ALIGNSIZESRC);
	TestCaps(flags, pFile, DDCAPS_ALIGNSTRIDE);
	TestCaps(flags, pFile, DDCAPS_ALPHA);
	TestCaps(flags, pFile, DDCAPS_BANKSWITCHED);
	TestCaps(flags, pFile, DDCAPS_BLT);
	TestCaps(flags, pFile, DDCAPS_BLTCOLORFILL);
	TestCaps(flags, pFile, DDCAPS_BLTDEPTHFILL);
	TestCaps(flags, pFile, DDCAPS_BLTFOURCC);
	TestCaps(flags, pFile, DDCAPS_BLTQUEUE);
	TestCaps(flags, pFile, DDCAPS_BLTSTRETCH);
	TestCaps(flags, pFile, DDCAPS_CANBLTSYSMEM);
	TestCaps(flags, pFile, DDCAPS_CANCLIP);
	TestCaps(flags, pFile, DDCAPS_CANCLIPSTRETCHED);
	TestCaps(flags, pFile, DDCAPS_COLORKEY);
	TestCaps(flags, pFile, DDCAPS_GDI);
	TestCaps(flags, pFile, DDCAPS_NOHARDWARE);
	TestCaps(flags, pFile, DDCAPS_OVERLAY);
	TestCaps(flags, pFile, DDCAPS_OVERLAYCANTCLIP);
	TestCaps(flags, pFile, DDCAPS_OVERLAYFOURCC);
	TestCaps(flags, pFile, DDCAPS_OVERLAYSTRETCH);
	TestCaps(flags, pFile, DDCAPS_PALETTE);
	TestCaps(flags, pFile, DDCAPS_PALETTEVSYNC);
	TestCaps(flags, pFile, DDCAPS_READSCANLINE);
//	TestCaps(flags, pFile, DDCAPS_STEREOVIEW);
	TestCaps(flags, pFile, DDCAPS_VBI);
	TestCaps(flags, pFile, DDCAPS_ZBLTS);
	TestCaps(flags, pFile, DDCAPS_ZOVERLAYS);
}

void CDirectDrawWrapper::LogCaps2(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDCAPS2_AUTOFLIPOVERLAY);
	TestCaps(flags, pFile, DDCAPS2_CANBOBHARDWARE);
	TestCaps(flags, pFile, DDCAPS2_CANBOBINTERLEAVED);
	TestCaps(flags, pFile, DDCAPS2_CANBOBNONINTERLEAVED);
	TestCaps(flags, pFile, DDCAPS2_CANDROPZ16BIT);
	TestCaps(flags, pFile, DDCAPS2_CANFLIPODDEVEN);
	TestCaps(flags, pFile, DDCAPS2_CANRENDERWINDOWED);
	TestCaps(flags, pFile, DDCAPS2_CERTIFIED);
	TestCaps(flags, pFile, DDCAPS2_COLORCONTROLPRIMARY);
	TestCaps(flags, pFile, DDCAPS2_COLORCONTROLOVERLAY);
	TestCaps(flags, pFile, DDCAPS2_COPYFOURCC);
	TestCaps(flags, pFile, DDCAPS2_FLIPINTERVAL);
	TestCaps(flags, pFile, DDCAPS2_FLIPNOVSYNC);
	TestCaps(flags, pFile, DDCAPS2_NO2DDURING3DSCENE);
	TestCaps(flags, pFile, DDCAPS2_NONLOCALVIDMEM);
	TestCaps(flags, pFile, DDCAPS2_NONLOCALVIDMEMCAPS);
	TestCaps(flags, pFile, DDCAPS2_NOPAGELOCKREQUIRED);
	TestCaps(flags, pFile, DDCAPS2_PRIMARYGAMMA);
	TestCaps(flags, pFile, DDCAPS2_VIDEOPORT);
	TestCaps(flags, pFile, DDCAPS2_WIDESURFACES);
}

void CDirectDrawWrapper::LogKeyCaps(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDCKEYCAPS_DESTBLT);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTBLTCLRSPACE);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTBLTCLRSPACEYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTBLTYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTOVERLAY);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTOVERLAYCLRSPACE);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTOVERLAYCLRSPACEYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTOVERLAYONEACTIVE);
	TestCaps(flags, pFile, DDCKEYCAPS_DESTOVERLAYYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_NOCOSTOVERLAY);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCBLT);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCBLTCLRSPACE);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCBLTCLRSPACEYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCBLTYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCOVERLAY);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCOVERLAYCLRSPACE);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCOVERLAYCLRSPACEYUV);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCOVERLAYONEACTIVE);
	TestCaps(flags, pFile, DDCKEYCAPS_SRCOVERLAYYUV);
}

void CDirectDrawWrapper::LogFxCaps(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDFXCAPS_BLTALPHA);
	TestCaps(flags, pFile, DDFXCAPS_BLTARITHSTRETCHY);
	TestCaps(flags, pFile, DDFXCAPS_BLTARITHSTRETCHYN);
	TestCaps(flags, pFile, DDFXCAPS_BLTFILTER);
	TestCaps(flags, pFile, DDFXCAPS_BLTMIRRORLEFTRIGHT);
	TestCaps(flags, pFile, DDFXCAPS_BLTMIRRORUPDOWN);
	TestCaps(flags, pFile, DDFXCAPS_BLTROTATION);
	TestCaps(flags, pFile, DDFXCAPS_BLTROTATION90);
	TestCaps(flags, pFile, DDFXCAPS_BLTSHRINKX);
	TestCaps(flags, pFile, DDFXCAPS_BLTSHRINKXN);
	TestCaps(flags, pFile, DDFXCAPS_BLTSHRINKY);
	TestCaps(flags, pFile, DDFXCAPS_BLTSHRINKYN);
	TestCaps(flags, pFile, DDFXCAPS_BLTSTRETCHX);
	TestCaps(flags, pFile, DDFXCAPS_BLTSTRETCHXN);
	TestCaps(flags, pFile, DDFXCAPS_BLTSTRETCHY);
	TestCaps(flags, pFile, DDFXCAPS_BLTSTRETCHYN);
//	TestCaps(flags, pFile, DDFXCAPS_BLTTRANSFORM);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYALPHA);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYARITHSTRETCHY);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYARITHSTRETCHYN);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYFILTER);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYMIRRORLEFTRIGHT);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYMIRRORUPDOWN);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSHRINKX);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSHRINKXN);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSHRINKY);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSHRINKYN);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSTRETCHX);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSTRETCHXN);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSTRETCHY);
	TestCaps(flags, pFile, DDFXCAPS_OVERLAYSTRETCHYN);
//	TestCaps(flags, pFile, DDFXCAPS_OVERLAYTRANSFORM);
}

void CDirectDrawWrapper::LogAlphaCaps(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDFXALPHACAPS_BLTALPHAEDGEBLEND);
	TestCaps(flags, pFile, DDFXALPHACAPS_BLTALPHAPIXELS);
	TestCaps(flags, pFile, DDFXALPHACAPS_BLTALPHAPIXELSNEG);
	TestCaps(flags, pFile, DDFXALPHACAPS_BLTALPHASURFACES);
	TestCaps(flags, pFile, DDFXALPHACAPS_BLTALPHASURFACESNEG);
	TestCaps(flags, pFile, DDFXALPHACAPS_OVERLAYALPHAEDGEBLEND);
	TestCaps(flags, pFile, DDFXALPHACAPS_OVERLAYALPHAPIXELS);
	TestCaps(flags, pFile, DDFXALPHACAPS_OVERLAYALPHAPIXELSNEG);
	TestCaps(flags, pFile, DDFXALPHACAPS_OVERLAYALPHASURFACES);
	TestCaps(flags, pFile, DDFXALPHACAPS_OVERLAYALPHASURFACESNEG);
}

void CDirectDrawWrapper::LogPaletteCaps(DWORD flags, FILE *pFile)
{
	TestCaps(flags, pFile, DDPCAPS_8BIT);
	TestCaps(flags, pFile, DDPCAPS_8BITENTRIES);
	TestCaps(flags, pFile, DDPCAPS_ALPHA);
	TestCaps(flags, pFile, DDPCAPS_ALLOW256);
	TestCaps(flags, pFile, DDPCAPS_PRIMARYSURFACE);
	TestCaps(flags, pFile, DDPCAPS_PRIMARYSURFACELEFT);
	TestCaps(flags, pFile, DDPCAPS_VSYNC);
	TestCaps(flags, pFile, DDPCAPS_1BIT);
	TestCaps(flags, pFile, DDPCAPS_2BIT);
	TestCaps(flags, pFile, DDPCAPS_4BIT);
}

*/
