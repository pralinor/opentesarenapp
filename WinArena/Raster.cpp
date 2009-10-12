/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Raster.cpp 10    12/31/06 10:55a Lee $
//
//	File: Raster.cpp
//
//
//	This is a software rasterizer.  All rendering is done in software, and
//	blitted to the screen using StretchDIBits().  The whole reason I started
//	writing WinArena was an excluse to write a software renderer that could
//	run in real time, so don't ask why I didn't use DirectX or some other
//	hardware API.  (Actually, I have done it with DirectX (partially), 64x64
//	DOS-level textures look even worse there, and no I can't send you that
//	code.)  I find the retro look of software rendering with blocky textures
//	suits this game just fine, and I had fun finding out that I could still
//	remember how to write a software renderer after not having looked at one
//	in more than a decade.
//
//	All of the math in this code is done with fixed-point numbers (17.15 format,
//	unless someone's changed the constants).  Some acceleration tricks are done
//	to speed up performance, but I have not delved into my dusty bag of tricks
//	-- many of which would likely lower performance, given how systems have
//	changed over the years.
//
//	WARNING: Do not attempt to run this code at any resolution higher than
//	960x600 (1280x800 might work, is untested, and would yield incredibly
//	low frame rates).  There are hard limits on numerical ranges when using
//	fixed-point math, and trying to exceed those limits will cause visual
//	glitches and crash the program with overflow/underflow/div-zero errors.
//	Given the low resolution of all the textures used, trying to render at a
//	higher resolution won't pay off visually, either.  When the window is set
//	at a higher resolution, leave it to StretchDIBits() to resize the frame
//	as required.
//
/////////////////////////////////////////////////////////////////////////////


#include "WinArena.h"
#include "Raster.h"
#include "TGA.h"
#include "GIFFile.h"


//	Light mapping is accomplished with palette tricks.  There are two lightmap
//	files in the Arena directory (NORMAL.LGT and FOG.LGT).  These are 2D arrays
//	that take a light value and color value, and yield a suitable darker color
//	from the colormap.
//
//	This is made more complex because Arena dithers color values based on the
//	(x,y) screen coordinate of a pixel.  It is still unclear how Arena does
//	this, or whether it uses the DITHER?.IMG files.  This code will XOR the x
//	and y coords, and use the low bits to offset the light value.  The end
//	result is an offset into the NORMAL.LGT buffer.  Add the color palette value
//	to the result of LumaIndex() to locate the remapped color value.
//
//#define LumaIndex(l,x,y)	(((l >> (c_FixedPointBits - 8)) + (((x ^ y) & 3) << 7)) & 0x1F00)
#define LumaIndex(l,x,y)	(((l >> (c_FixedPointBits - 8)) + (((x&1)<<6) | (((x^y)&1)<<7))) & 0x1F00)
//#define LumaIndex(l,x,y)	(((l >> (c_FixedPointBits - 8)) + (1 << 7)) & 0x1F00)

/*
long g_LumaLut[16] =
{
	0<<7, 1<<7, 2<<7, 3<<7,
	1<<7, 0<<7, 3<<7, 2<<7,
	2<<7, 3<<7, 0<<7, 1<<7,
	3<<7, 2<<7, 1<<7, 0<<7
};
#define LumaIndex(l,x,y)	(((l >> (c_FixedPointBits - 8)) + g_LumaLut[(x&3) | ((y&3)<<2)]) & 0x1F00)
*/
//#define LumaIndex(l,x,y)	((((l * 16) >> (c_FixedPointBits - 8)) ) & 0x1F00)

//	Acceleration trick: Precompute the inverse-Z divisors.  This is rather granular,
//	and looks good with some of the rendering code, but it does remove a rather
//	expensive perspective division from some inner rendering loops.  This turns
//	the perspective division into multiplication, which on my system boosts frame
//	rate by 20% at 640x400.
//
#define c_InvBits	6
long g_InvDiv[256*32];


//	Macrotize the perspective division into the equivalent multiplication.
//
#define FastInvZ(n,d)		FixedMul(n, g_InvDiv[d >> (c_FixedPointBits - c_InvBits)])


/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
CwaRaster::CwaRaster(void)
	:	m_pPrivateFrame(NULL),
		m_pFrame(NULL),
		m_pPalette(NULL),
		m_pLightMap(NULL),
		m_pShapeID(NULL),
		m_pDepth(NULL),
		m_pTexture(NULL),
		m_BlitMode(WHITEONBLACK)
{
	// Create a default 32x32 checkerboard texture.  This will be used if no
	// other texture has been explicitly provided, which avoids needing to
	// add any special logic to deal with m_pTexture being NULL.
	//
	// This was used in testing, and should not currently appear (unless a
	// texture load fails).
	//
	m_DefaultTexture.Allocate(32, 32);

	for (DWORD y = 0; y <= m_DefaultTexture.m_Height; ++y) {
		for (DWORD x = 0; x <= m_DefaultTexture.m_Width; ++x) {
			if ((x & 2) == (y & 2)) {
				m_DefaultTexture.m_pTexels[y*m_DefaultTexture.m_Pitch + x] = 16;
			}
			else {
				m_DefaultTexture.m_pTexels[y*m_DefaultTexture.m_Pitch + x] = 0;
			}
		}
	}

	// Precompute the inverse perspective division values, allowing a multiply
	// to be done instead of a more expensive division.
	g_InvDiv[0] = c_FixedPointOne;
	for (DWORD i = 1; i < ArraySize(g_InvDiv); ++i) {
		DWORD denom = IntToFixed(i) >> c_InvBits;
		g_InvDiv[i] = FixedDiv(c_FixedPointOne, denom);
	}

	// Always keep m_pTexture pointing towards something.
	m_pTexture = &m_DefaultTexture;

	InitFixedDistanceLUT();
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
CwaRaster::~CwaRaster(void)
{
	SafeDeleteArray(m_pPrivateFrame);
	SafeDeleteArray(m_pShapeID);
	SafeDeleteArray(m_pDepth);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Allocate()
//
//	Reallocates the all buffers used during rasterization.  This code is
//	called any time the user switches rendering resolution.
//
//	NOTE: This code is not called when the app's window is resized, only
//	when the rendering resolution is changed.
//
void CwaRaster::Allocate(DWORD width, DWORD height)
{
	SafeDeleteArray(m_pPrivateFrame);
	SafeDeleteArray(m_pShapeID);
	SafeDeleteArray(m_pDepth);

	// Full size of rendering buffer.
	m_FramePitch	= width;
	m_FrameWidth	= width;
	m_FrameHeight	= height;
	m_PixelCount	= m_FrameWidth * m_FrameHeight;

	// View area of buffer, which is where 3D rendering is performed.
	// Everything below line 146 is part of the HUD.
	m_ViewWidth		= 320;
	m_ViewHeight	= 146;

	if (m_FrameHeight >= 600) {
		m_ViewWidth  *= 3;
		m_ViewHeight *= 3;
	}
	else if (m_FrameHeight >= 400) {
		m_ViewWidth  *= 2;
		m_ViewHeight *= 2;
	}

	m_pPrivateFrame	= new BYTE[m_PixelCount];
	m_pShapeID		= new DWORD[m_PixelCount];
	m_pDepth		= new long[m_PixelCount];

	m_pFrame = m_pPrivateFrame;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetWindowSize()
//
//	This is called any time the window is resized.  The width/height pair
//	are cached and used when blitting the video frame to the window.
//
void CwaRaster::SetWindowSize(DWORD windowWidth, DWORD windowHeight)
{
	m_WindowWidth  = windowWidth;
	m_WindowHeight = windowHeight;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetBlitMode()
//
//	Controls the blitting mode used by StretchDIBits().  There are a couple
//	different modes that should be used: BLACKONWHITE, which is fast but
//	crappy if it needs to resize down, and HALFTONE, which looks much better
//	when resizing, but is usually much slower, depending upon the system.
//
void CwaRaster::SetBlitMode(DWORD mode)
{
    m_BlitMode = mode;
}


void CwaRaster::SetFrameBuffer(BYTE *pBuffer, DWORD pitch)
{
	if (NULL == pBuffer) {
		m_pFrame     = m_pPrivateFrame;
		m_FramePitch = m_FrameWidth;
	}
	else {
		m_pFrame     = pBuffer;
		m_FramePitch = pitch;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ClearFrameBuffer()
//
//	Clears out the frame buffer.
//
void CwaRaster::ClearFrameBuffer(DWORD color, DWORD lineCount)
{
	m_pPalette = g_pArenaLoader->GetPaletteARGB(Palette_Default);

	if (0 == lineCount) {
		lineCount = m_FrameHeight;
	}

	DWORD bits     = color | (color << 8) | (color << 16) | (color << 24);
	DWORD pixCount = m_FrameWidth >> 2;

	BYTE *pLine = m_pFrame;

	for (DWORD y = 0; y < lineCount; ++y) {
		for (DWORD x = 0; x < pixCount; ++x) {
			reinterpret_cast<DWORD*>(pLine)[x] = bits;
		}
		pLine += m_FramePitch;
	}
//	memset(m_pFrame, color, m_FramePitch * lineCount);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ClearDepthBuffer()
//
//	Clears out the depth and shape ID buffers.
//
void CwaRaster::ClearDepthBuffer(void)
{
	for (DWORD i = 0; i < m_PixelCount; ++i) {
		m_pShapeID[i] = 0;
		m_pDepth[i]   = 0;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScreenShot()
//
//	Dumps a copy of the current frame buffer off to a GIF file.
//
void CwaRaster::ScreenShot(char filename[])
{
	CGIFFile file;

	GIFFrameBuffer frame;
	frame.Width      = m_FrameWidth;
	frame.Height     = m_FrameHeight;
	frame.Pitch      = m_FramePitch;
	frame.BufferSize = m_FramePitch * m_FrameHeight;
	frame.pBuffer    = m_pFrame;
	frame.ColorMapEntryCount = 256;

	for (DWORD i = 0; i < 256; ++i) {
		frame.ColorMap[i][0] = BYTE(m_pPalette[i] >> 16);
		frame.ColorMap[i][1] = BYTE(m_pPalette[i] >>  8);
		frame.ColorMap[i][2] = BYTE(m_pPalette[i]      );
	}

	file.Open(filename, false);
	file.SetCodeSize(8);
	file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
	file.InitOutput(m_FrameWidth, m_FrameHeight);
	file.WriteImage(frame);
	file.Close();

/*
	TGAFileHeader header;
	memset(&header, 0, sizeof(header));
	header.ImageType                 = TGAImageType_ColorMapped;
	header.ColorMapType              = 1;
	header.ColorMapSpec.Length       = 256;
	header.ColorMapSpec.EntrySize    = 24;
	header.ImageSpec.ImageDescriptor = TGATopToBottom;
	header.ImageSpec.ImageWidth      = WORD(m_FrameWidth);
	header.ImageSpec.ImageHeight     = WORD(m_FrameHeight);
	header.ImageSpec.PixelDepth      = 8;

	BYTE palette[3*256];
	GetPaletteBlueFirst(Palette_Default, palette);

	TGAFooter footer;
	footer.devDirOffset  = 0;
	footer.extAreaOffset = 0;
	strcpy(footer.signature, TGASignature);

	FILE *pFile = fopen(filename, "wb");
	if (NULL != pFile) {
		fwrite(&header, 1, sizeof(header), pFile);
		fwrite(palette, 1, 3 * 256, pFile);
		fwrite(m_pFrame, 1, m_PixelCount, pFile);
		fwrite(&footer, 1, sizeof(footer), pFile);

		fclose(pFile);
	}
*/
}


/////////////////////////////////////////////////////////////////////////////
//
//	DisplayFrame()
//
//	Blits the current frame buffer to the window.  If any resizing of the
//	image is required, StretchDIBits will take care of it.
//
void CwaRaster::DisplayFrame(HDC hDC)
{
	PROFILE(PFID_Raster_DisplayFrame);

	if ((NULL != m_pPalette) && (NULL != m_pFrame)) {
		DWORD headerSize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);

		BITMAPINFO *pInfo = reinterpret_cast<BITMAPINFO*>(alloca(headerSize));

		memset(pInfo, 0, sizeof(BITMAPINFOHEADER));
		pInfo->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
		pInfo->bmiHeader.biPlanes		= 1;
		pInfo->bmiHeader.biBitCount		= WORD(8);
		pInfo->bmiHeader.biCompression	= BI_RGB;
		pInfo->bmiHeader.biWidth		= m_FrameWidth;
		pInfo->bmiHeader.biHeight		= -long(m_FrameHeight);
		pInfo->bmiHeader.biSizeImage	= m_FramePitch * m_FrameHeight;
		pInfo->bmiHeader.biClrImportant = 256;
		pInfo->bmiHeader.biClrUsed		= 256;

		memcpy(pInfo->bmiColors, m_pPalette, 256 * sizeof(RGBQUAD));

		SetStretchBltMode(hDC, m_BlitMode);
		StretchDIBits(hDC, 0, 0, m_WindowWidth, m_WindowHeight, 0, 0, m_FrameWidth, m_FrameHeight, m_pFrame, pInfo, DIB_RGB_COLORS, SRCCOPY);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawBox()
//
//	Draws a colored outline into the frame buffer.
//
//	No bounds checking is done, so out-of-bounds sizes will violate memory.
//
void CwaRaster::DrawBox(long x, long y, long width, long height, BYTE color)
{
	BYTE *pLine1 = m_pFrame + (y * m_FramePitch) + x;
	BYTE *pLine2 = pLine1 + ((height - 1) * m_FramePitch);

	long i;

	for (i = 0; i < width; ++i) {
		pLine1[i] = color;
		pLine2[i] = color;
	}

	pLine1 = m_pFrame + ((y + 1) * m_FramePitch) + x;
	pLine2 = pLine1 + (width - 1);

	for (i = 1; i < height; ++i) {
		*pLine1 = color;
		*pLine2 = color;
		pLine1 += m_FramePitch;
		pLine2 += m_FramePitch;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	Line()
//
//	Bresenham a line rom (x0,y0) to (x1,y1).
//
//	This code is obsolete, from a much earlier chunk of rendering code, but
//	kept around in case it should prove useful.
//
void CwaRaster::Line(long x0, long y0, long x1, long y1, BYTE color)
{
	// Calculate deltax and deltay for initialization.
	long deltax = abs(x1 - x0);
	long deltay = abs(y1 - y0);

	long numpixels, d, dinc0, dinc1, xinc0, xinc1, yinc0, yinc1;

	// Initialize all values, based on which is the independent variable.
	if (deltax >= deltay) {
		// x is independent variable
		numpixels = deltax + 1;
		d     = (2 * deltay) - deltax;
		dinc0 = deltay << 1;
		dinc1 = (deltay - deltax) << 1;
		xinc0 = 1;
		xinc1 = 1;
		yinc0 = 0;
		yinc1 = 1;
	}
	else {
		// y is independent variable
		numpixels = deltay + 1;
		d = (2 * deltax) - deltay;
		dinc0 = deltax << 1;
		dinc1 = (deltax - deltay) << 1;
		xinc0 = 0;
		xinc1 = 1;
		yinc0 = 1;
		yinc1 = 1;
	}

	// Make sure x and y move in the correct directions.
	if (x0 > x1) {
		xinc0 = -xinc0;
		xinc1 = -xinc1;
	}
	if (y0 > y1) {
		yinc0 = -yinc0;
		yinc1 = -yinc1;
    }

	// Start drawing here.
	long x = x0;
	long y = y0;

	// Draw the pixels, looping over the dependent axis, with one pixel
	// being plotted per row/column.
	for (long i = 0; i < numpixels; ++i) {
		m_pFrame[(y * m_FramePitch) + x] = color;

		if (d < 0) {
			d = d + dinc0;
			x = x + xinc0;
			y = y + yinc0;
		}
		else {
			d = d + dinc1;
			x = x + xinc1;
			y = y + yinc1;
		}
	}
}


#define PutPixel(x,y,c) { m_pFrame[((y)*m_FramePitch)+(x)] = c; }


/////////////////////////////////////////////////////////////////////////////
//
//	Circle()
//
//	Draws a circle into the frame buffer.  No bounds checking of values is
//	done.
//
//	This code is obsolete, from a much earlier chunk of rendering code, but
//	kept around in case it should prove useful.
//
void CwaRaster::Circle(long x0, long y0, long radius, BYTE color)
{
	long d = 3 - (2 * radius);

	long y = radius;

	for (long x = 0; x < y; ++x) {
		if (d < 0) {
		    d = d + (4 * x) + 6;
		}
		else {
		    d = d + 4 * (x - y) + 10;
			y = y - 1;
		}

		PutPixel(x0 + x, y0 + y, color);
		PutPixel(x0 + x, y0 - y, color);
		PutPixel(x0 - x, y0 + y, color);
		PutPixel(x0 - x, y0 - y, color);
		PutPixel(x0 + y, y0 + x, color);
		PutPixel(x0 + y, y0 - x, color);
		PutPixel(x0 - y, y0 + x, color);
		PutPixel(x0 - y, y0 - x, color);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	FillRect()
//
void CwaRaster::FillRect(long x, long y, long width, long height, long scale, BYTE color)
{
	PROFILE(PFID_Raster_FillRect);

	BYTE *pLine = m_pFrame + (scale * (x + (y * m_FramePitch)));

	width  *= scale;
	height *= scale;

	for (long lineNum = 0; lineNum < height; ++lineNum) {
		memset(pLine, color, width);
		pLine += m_FramePitch;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	Blit()
//
//	Blits the given texture into the frame buffer at the (posX,posY) position
//	in the frame buffer.  Cropping is applied if the texture would extend off
//	the bottom or right side of the frame buffer.
//
void CwaRaster::Blit(const CwaTexture &texture, DWORD posX, DWORD posY)
{
	PROFILE(PFID_Raster_Blit);

	DWORD width  = texture.m_Width;
	DWORD height = texture.m_Height;

	// At 960x600, render each pixel 3x3 times.
	if (m_FrameHeight >= 600) {
		width  = width  * 3;
		height = height * 3;
	}

	// At 640x400, render each pixel 2x2 times.
	else if (m_FrameHeight >= 400) {
		width  = width  << 1;
		height = height << 1;
	}

	if (posX >= m_FrameWidth) {
		width = 0;
	}
	else if ((posX + width) > m_FrameWidth) {
		width = m_FrameWidth - posX;
	}

	if (posY >= m_FrameHeight) {
		height = 0;
	}
	else if ((posY + height) > m_FrameHeight) {
		height = m_FrameHeight - posY;
	}

	if (NULL != texture.m_pPalette) {
		m_pPalette = texture.m_pPalette;
	}

	// 1:1 case for 320x200
	if (m_FrameHeight < 400) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + (y * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					pDst[x] = pSrc[x];
				}
			}
		}
	}

	// 1:2 case for 640x400
	else if (m_FrameHeight < 600) {
		width = width >> 1;

		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y >> 1) * texture.m_Pitch);
				WORD *pDst = reinterpret_cast<WORD*>(m_pFrame + ((y + posY) * m_FramePitch) + posX);

				for (DWORD x = 0; x < width; ++x) {
					pDst[x] = WORD(pSrc[x]) | (WORD(pSrc[x]) << 8);
				}
			}
		}
	}

	// 1:3 case for 960x600
	else {
		if ((width > 0) && (height > 0)) {
			DWORD frac = width % 3;

			width = width / 3;

			for (DWORD y = 0; y < height; ++y) {

				BYTE *pSrc = texture.m_pTexels + ((y / 3) * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				// Avoid dividing by 3 in the inner loop.  This increases FPS
				// by 5% when staring at a wall.

				DWORD a = 0, x = 0;
				for (; x < width; a +=3, ++x) {
					pDst[a  ] = pSrc[x];
					pDst[a+1] = pSrc[x];
					pDst[a+2] = pSrc[x];
				}

				// Due to cropping, there may be 1 or 2 extra pixels that
				// still need to be rendered to the right edge of the screen.
				for ( ; frac > 0; --frac) {
					pDst[a++] = pSrc[x];
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	BlitRect()
//
//	This is pretty much the same as Blit(), except it allows blitting a
//	sub-region out of the texture to the frame buffer.
//
//	(posX,posY)  - position in frame buffer where image will be blitted
//	(dx,dy)      - upper-left corner of sub-rect in texture
//	width/height - dimensions of sub-rect to blit, in screen resolution units
//
void CwaRaster::BlitRect(const CwaTexture &texture, DWORD posX, DWORD posY, DWORD dx, DWORD dy, DWORD width, DWORD height)
{
	PROFILE(PFID_Raster_BlitRect);

	if (posX >= m_FrameWidth) {
		width = 0;
	}
	else if ((posX + width) > m_FrameWidth) {
		width = m_FrameWidth - posX;
	}

	if (posY >= m_FrameHeight) {
		height = 0;
	}
	else if ((posY + height) > m_FrameHeight) {
		height = m_FrameHeight - posY;
	}


	if (m_FrameHeight < 400) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y + dy) * texture.m_Pitch) + dx;
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					pDst[x] = pSrc[x];
				}
			}
		}
	}
	else if (m_FrameHeight < 600) {
		width = width >> 1;
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + (((y + dy) >> 1) * texture.m_Pitch) + dx;
				WORD *pDst = reinterpret_cast<WORD*>(m_pFrame + ((y + posY) * m_FramePitch) + posX);

				for (DWORD x = 0; x < width; ++x) {
					pDst[x] = pSrc[x] | (WORD(pSrc[x]) << 8);
				}
			}
		}
	}
	else {
		if ((width > 0) && (height > 0)) {
			DWORD frac = width % 3;

			width = width / 3;

			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + (((y + dy) / 3) * texture.m_Pitch) + dx;
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				// Avoid dividing by 3 in the inner loop.  This increases FPS
				// by 5% when staring at a wall.

				DWORD a = 0, x = 0;
				for (; x < width; a +=3, ++x) {
					pDst[a  ] = pSrc[x];
					pDst[a+1] = pSrc[x];
					pDst[a+2] = pSrc[x];
				}

				for ( ; frac > 0; --frac) {
					pDst[a++] = pSrc[x];
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	BlitTransparent()
//
//	Same as Blit(), except that transparency test is applied in the inner
//	loop.  Color zero is considered to be transparent.  All other colors
//	are opaque.
//
void CwaRaster::BlitTransparent(const CwaTexture &texture, DWORD posX, DWORD posY)
{
	PROFILE(PFID_Raster_BlitTransparent);

	DWORD width  = texture.m_Width;
	DWORD height = texture.m_Height;

	if (m_FrameHeight >= 600) {
		width  = width  * 3;
		height = height * 3;
	}
	else if (m_FrameHeight >= 400) {
		width  = width  << 1;
		height = height << 1;
	}

	if (posX >= m_FrameWidth) {
		width = 0;
	}
	else if ((posX + width) > m_FrameWidth) {
		width = m_FrameWidth - posX;
	}

	if (posY >= m_FrameHeight) {
		height = 0;
	}
	else if ((posY + height) > m_FrameHeight) {
		height = m_FrameHeight - posY;
	}

	if (m_FrameHeight < 400) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + (y * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					if (pSrc[x]) {
						pDst[x] = pSrc[x];
					}
				}
			}
		}
	}
	else if (m_FrameHeight < 600) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y >> 1) * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					if (pSrc[x >> 1]) {
						pDst[x] = pSrc[x >> 1];
					}
				}
			}
		}
	}
	else {
		if ((width > 0) && (height > 0)) {
			DWORD frac = width % 3;

			width = width / 3;

			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y / 3) * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				// Avoid dividing by 3 in the inner loop.

				DWORD a = 0, x = 0;
				for ( ; x < width; a +=3, ++x) {
					BYTE foo = pSrc[x];

					if (foo) {
						pDst[a  ] = pSrc[x];
						pDst[a+1] = pSrc[x];
						pDst[a+2] = pSrc[x];
					}
				}

				if (frac > 0) {
					BYTE foo = pSrc[x];

					if (foo) {
						for ( ; frac > 0; --frac) {
							pDst[a++] = pSrc[x];
						}
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	BlitTransparentMirror()
//
//	Same as BlitTransparent(), except that it flips the texture left-to-right.
//	This is used to render the off-side of monster textures.  The monster
//	animations only show one side of the monster, so to display a monster
//	from any direction, some animations need to be reversed when rendering.
//
void CwaRaster::BlitTransparentMirror(const CwaTexture &texture, DWORD posX, DWORD posY)
{
	PROFILE(PFID_Raster_BlitTransparentMirror);

	DWORD width  = texture.m_Width;
	DWORD height = texture.m_Height;

	if (m_FrameHeight >= 600) {
		width  = width  * 3;
		height = height * 3;
	}
	else if (m_FrameHeight >= 400) {
		width  = width  << 1;
		height = height << 1;
	}

	if (posX >= m_FrameWidth) {
		width = 0;
	}
	else if ((posX + width) > m_FrameWidth) {
		width = m_FrameWidth - posX;
	}

	if (posY >= m_FrameHeight) {
		height = 0;
	}
	else if ((posY + height) > m_FrameHeight) {
		height = m_FrameHeight - posY;
	}

	if (m_FrameHeight < 400) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + (y * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					register DWORD ix = width - x - 1;

					if (pSrc[ix]) {
						pDst[x] = pSrc[ix];
					}
				}
			}
		}
	}
	else if (m_FrameHeight < 600) {
		if ((width > 0) && (height > 0)) {
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y >> 1) * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				for (DWORD x = 0; x < width; ++x) {
					register DWORD ix = (width - x - 1) >> 1;

					if (pSrc[ix]) {
						pDst[x] = pSrc[ix];
					}
				}
			}
		}
	}
	else {
		if ((width > 0) && (height > 0)) {
			DWORD frac = width % 3;

			width = width / 3;

			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = texture.m_pTexels + ((y / 3) * texture.m_Pitch);
				BYTE *pDst = m_pFrame + ((y + posY) * m_FramePitch) + posX;

				// Avoid dividing by 3 in the inner loop.
				// NOTE: (x < width) is safe with --x since -1 == really big
				// unsigned value.

				DWORD a = 0, x = width - 1;
				for ( ; x < width; a +=3, --x) {
					BYTE foo = pSrc[x];

					if (foo) {
						pDst[a  ] = pSrc[x];
						pDst[a+1] = pSrc[x];
						pDst[a+2] = pSrc[x];
					}
				}

				if ((frac > 0) && (x < width)) {
					BYTE foo = pSrc[x];

					if (foo) {
						for ( ; frac > 0; --frac) {
							pDst[a++] = pSrc[x];
						}
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintVerticalQuad()
//
void CwaRaster::PaintVerticalQuad(Vertex_t corners[])
{
	PROFILE(PFID_Raster_PaintVerticalQuad);

	long xLeft  = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long xRight = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;

if (NULL == m_pTexture->m_pTexels) return;

	if ((xLeft < xRight) && (xRight > 0) && (xLeft < long(m_ViewWidth))) {
		long length = corners[1].x - corners[0].x;

		if (length < c_FixedPointOne) {
			length = c_FixedPointOne;
		}

		long* yMin = reinterpret_cast<long*>(alloca((xRight-xLeft+1) * sizeof(long)));
		long* yMax = reinterpret_cast<long*>(alloca((xRight-xLeft+1) * sizeof(long)));

		long cx0 = corners[0].x;
		long cx1 = corners[1].x;
		long cy0 = corners[0].y;
		long cy1 = corners[1].y;
		long *pym = yMin;

		for (DWORD q = 2; q > 0; --q) {
			if (cy0 < cy1) {
				long yT = (cy0 + c_FixedPointMask) >> c_FixedPointBits;
				long yB = (cy1 + c_FixedPointMask) >> c_FixedPointBits;

				long x = xLeft;
				for (long y = yT; y < yB; ++y) {
					long mulL = FixedDiv(((y << c_FixedPointBits) - cy0), (cy1 - cy0));
					long xEnd = FixedMul((cx1 - cx0), mulL) + cx0;
					xEnd = (xEnd + c_FixedPointMask) >> c_FixedPointBits;
					if (xEnd > xRight) {
						xEnd = xRight;
					}
					while (x < xEnd) {
						pym[x - xLeft] = y;
						++x;
					}
				}
				while (x < xRight) {
					pym[x - xLeft] = yB;
					++x;
				}
			}
			else {
				long yT = (cy1 + c_FixedPointMask) >> c_FixedPointBits;
				long yB = (cy0 + c_FixedPointMask) >> c_FixedPointBits;

				long x = xRight - 1;
				for (long y = yT; y < yB; ++y) {
					long mulL = FixedDiv(((y << c_FixedPointBits) - cy1), (cy0 - cy1));
					long xEnd = FixedMul((cx0 - cx1), mulL) + cx1;
					xEnd = (xEnd + c_FixedPointMask) >> c_FixedPointBits;
					if (xEnd < xLeft) {
						xEnd = xLeft;
					}
					while (x >= xEnd) {
						pym[x - xLeft] = y;
						--x;
					}
				}
				while (x >= xLeft) {
					pym[x - xLeft] = yB;
					--x;
				}
			}

			cx0 = corners[3].x;
			cx1 = corners[2].x;
			cy0 = corners[3].y;
			cy1 = corners[2].y;
			pym = yMax;
		}

		long zL = FixedDiv(c_FixedPointOne, corners[0].z);
		long zR = FixedDiv(c_FixedPointOne, corners[1].z);
		long uL = FixedMul(corners[0].u, zL);
		long uR = FixedMul(corners[1].u, zR);
		long lL = FixedMul(corners[0].l, zL);
		long lR = FixedMul(corners[1].l, zR);

		long yT  = corners[0].y;
		long vT  = corners[0].v << 10;
		long yB  = corners[3].y;
		long vB  = corners[3].v << 10;
		long yTd = FixedDiv((corners[1].y - corners[0].y), length);
		long yBd = FixedDiv((corners[2].y - corners[3].y), length);

		for (long x = xLeft; x < xRight; ++x) {
			if (DWORD(x) < m_ViewWidth) {
				long mul   = FixedDiv(((x << c_FixedPointBits) - corners[0].x), length);

				long zlerp = FixedMul((zR - zL), mul) + zL;
				long ulerp = FixedMul((uR - uL), mul) + uL;
				long llerp = FixedMul((lR - lL), mul) + lL;

				long u = FixedDiv(ulerp, zlerp);
				long l = FixedDiv(llerp, zlerp);

				u = (u * m_pTexture->m_Width) >> c_FixedPointBits;

				if (0xFFFFFFC0 & u) {
					if (u < 0) u = 0;
					else       u = 63;
				}

				long y0 = yMin[x - xLeft];
				long y1 = yMax[x - xLeft];

				if (y0 < y1) {
					long ylen = (yB - yT);
					if (ylen < c_FixedPointOne) {
						ylen = c_FixedPointOne;
					}

					long vd = FixedDiv((vB - vT), ylen);

					long v = vT + FixedMul(((y0 << c_FixedPointBits) - yT), vd);

					if (y0 < 0) {
						v  += vd * -y0;
						y0  = 0;
					}

					if (y1 > long(m_ViewHeight)) {
						y1 = m_ViewHeight;
					}

					BYTE *pColumn = m_pFrame + (y0 * m_FramePitch) + x;

					for (long y = y0; y < y1; ++y) {
						long v0 = long(v >> (c_FixedPointBits + 10 - 6));
						if (0xFFFFFFC0 & v0) {
							if (v0 < 0) v0 = 0;
							else        v0 = 63;
						}

						// Apply light-mapping to fade the color towards black.
						*pColumn = m_pLightMap[LumaIndex(l,x,y) | m_pTexture->GetPixel(u, v0)];

						pColumn += m_FramePitch;
						v       += vd;
					}
				}
			}

			yT += yTd;
			yB += yBd;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintVerticalQuadTransparent()
//
void CwaRaster::PaintVerticalQuadTransparent(Vertex_t corners[])
{
	PROFILE(PFID_Raster_PaintVerticalQuadTransparent);

	long xLeft  = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long xRight = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;

	if ((xLeft < xRight) && (xRight > 0) && (xLeft < long(m_ViewWidth))) {
		long length = corners[1].x - corners[0].x;

		if (length < c_FixedPointOne) {
			length = c_FixedPointOne;
		}

		long* yMin = reinterpret_cast<long*>(alloca((xRight-xLeft+1) * sizeof(long)));
		long* yMax = reinterpret_cast<long*>(alloca((xRight-xLeft+1) * sizeof(long)));

		long cx0 = corners[0].x;
		long cx1 = corners[1].x;
		long cy0 = corners[0].y;
		long cy1 = corners[1].y;
		long *pym = yMin;

		for (DWORD q = 2; q > 0; --q) {
			if (cy0 < cy1) {
				long yT = (cy0 + c_FixedPointMask) >> c_FixedPointBits;
				long yB = (cy1 + c_FixedPointMask) >> c_FixedPointBits;

				long x = xLeft;
				for (long y = yT; y < yB; ++y) {
					long mulL = FixedDiv(((y << c_FixedPointBits) - cy0), (cy1 - cy0));
					long xEnd = FixedMul((cx1 - cx0), mulL) + cx0;
					xEnd = (xEnd + c_FixedPointMask) >> c_FixedPointBits;
					if (xEnd > xRight) {
						xEnd = xRight;
					}
					while (x < xEnd) {
						pym[x - xLeft] = y;
						++x;
					}
				}
				while (x < xRight) {
					pym[x - xLeft] = yB;
					++x;
				}
			}
			else {
				long yT = (cy1 + c_FixedPointMask) >> c_FixedPointBits;
				long yB = (cy0 + c_FixedPointMask) >> c_FixedPointBits;

				long x = xRight - 1;
				for (long y = yT; y < yB; ++y) {
					long mulL = FixedDiv(((y << c_FixedPointBits) - cy1), (cy0 - cy1));
					long xEnd = FixedMul((cx0 - cx1), mulL) + cx1;
					xEnd = (xEnd + c_FixedPointMask) >> c_FixedPointBits;
					if (xEnd < xLeft) {
						xEnd = xLeft;
					}
					while (x >= xEnd) {
						pym[x - xLeft] = y;
						--x;
					}
				}
				while (x >= xLeft) {
					pym[x - xLeft] = yB;
					--x;
				}
			}

			cx0 = corners[3].x;
			cx1 = corners[2].x;
			cy0 = corners[3].y;
			cy1 = corners[2].y;
			pym = yMax;
		}

		long zL = FixedDiv(c_FixedPointOne, corners[0].z);
		long zR = FixedDiv(c_FixedPointOne, corners[1].z);
		long uL = FixedMul(corners[0].u, zL);
		long uR = FixedMul(corners[1].u, zR);
		long lL = FixedMul(corners[0].l, zL);
		long lR = FixedMul(corners[1].l, zR);

		long yT  = corners[0].y;
		long vT  = corners[0].v << 8;
		long yB  = corners[3].y;
		long vB  = corners[3].v << 8;
		long yTd = FixedDiv((corners[1].y - corners[0].y), length);
		long vTd = FixedDiv((corners[1].v - corners[0].v), length);
		long yBd = FixedDiv((corners[2].y - corners[3].y), length);
		long vBd = FixedDiv((corners[2].v - corners[3].v), length);

		for (long x = xLeft; x < xRight; ++x) {
			if (DWORD(x) < m_ViewWidth) {
				long mul   = FixedDiv(((x << c_FixedPointBits) - corners[0].x), length);

				long zlerp = FixedMul((zR - zL), mul) + zL;
				long ulerp = FixedMul((uR - uL), mul) + uL;
				long llerp = FixedMul((lR - lL), mul) + lL;

				long u = FixedDiv(ulerp, zlerp);
				long l = FixedDiv(llerp, zlerp);

				u = (u * m_pTexture->m_Width) >> c_FixedPointBits;

				if (u <  0) u =  0;
				if (u > 63) u = 63;

				long y0 = yMin[x - xLeft];
				long y1 = yMax[x - xLeft];

				if (y0 < y1) {
					long ylen = (yB - yT);
					if (ylen < c_FixedPointOne) {
						ylen = c_FixedPointOne;
					}

					long vd = FixedDiv((vB - vT), ylen);

					long v = vT + FixedMul(((y0 << c_FixedPointBits) - yT), vd);

					if (y0 < 0) {
						v  += vd * -y0;
						y0  = 0;
					}

					if (y1 > long(m_ViewHeight)) {
						y1 = m_ViewHeight;
					}

					BYTE *pColumn = m_pFrame + (y0 * m_FramePitch) + x;

					for (long y = y0; y < y1; ++y) {
						long v0 = (long(v * m_pTexture->m_Height) >> (c_FixedPointBits + 8));
						if (0xFFFFFFC0 & v0) {
							if (v0 < 0) v0 = 0;
							else        v0 = 63;
						}

						// Fetch the color-mapped value for this texture coord.
						register DWORD pixel = m_pTexture->GetPixel(u, v0);

						if (pixel) {
							// Apply light-mapping to fade the color towards black.
							*pColumn = m_pLightMap[LumaIndex(l,x,y) | pixel];
						}

						pColumn += m_FramePitch;
						v       += vd;
					}
				}
			}

			yT += yTd;
			vT += vTd;
			yB += yBd;
			vB += vBd;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintVerticalQuadDepthID()
//
void CwaRaster::PaintVerticalQuadDepthID(Vertex_t corners[], DWORD shapeID)
{
	PROFILE(PFID_Raster_PaintVerticalQuadDepthID);

	long xLeft  = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long xRight = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;

	if ((xLeft < xRight) && (xRight > 0) && (xLeft < long(m_ViewWidth))) {
		long length = corners[1].x - corners[0].x;

		if (length < c_FixedPointOne) {
			length = c_FixedPointOne;
		}

		long zL = FixedDiv(c_FixedPointOne, corners[0].z);
		long zR = FixedDiv(c_FixedPointOne, corners[1].z);
		long uL = FixedMul(corners[0].u, zL);
		long uR = FixedMul(corners[1].u, zR);

		long yT  = corners[0].y;
		long vT  = corners[0].v << 8;
		long yB  = corners[3].y;
		long vB  = corners[3].v << 8;
		long yTd = FixedDiv((corners[1].y - corners[0].y), length);
		long vTd = FixedDiv((corners[1].v - corners[0].v), length);
		long yBd = FixedDiv((corners[2].y - corners[3].y), length);
		long vBd = FixedDiv((corners[2].v - corners[3].v), length);

		for (long x = xLeft; x < xRight; ++x) {
			if (DWORD(x) < m_ViewWidth) {
				long mul   = FixedDiv(((x << c_FixedPointBits) - corners[0].x), length);

				long zlerp = FixedMul((zR - zL), mul) + zL;
				long ulerp = FixedMul((uR - uL), mul) + uL;

				long u = FixedDiv(ulerp, zlerp);

				u = (u * m_pTexture->m_Width) >> c_FixedPointBits;

				long y0 = (yT + c_FixedPointMask) >> c_FixedPointBits;
				long y1 = (yB + c_FixedPointMask) >> c_FixedPointBits;

				if (y0 < y1) {
					long ylen = (yB - yT);
					if (ylen < c_FixedPointOne) {
						ylen = c_FixedPointOne;
					}

					long vd = FixedDiv((vB - vT), ylen);

					long v = vT + FixedMul(((y0 << c_FixedPointBits) - yT), vd);

					if (y0 < 0) {
						v  += vd * -y0;
						y0  = 0;
					}

					if (y1 > long(m_ViewHeight)) {
						y1 = m_ViewHeight;
					}

					DWORD *pShape = m_pShapeID + (y0 * m_FramePitch) + x;
					long  *pDepth = m_pDepth   + (y0 * m_FramePitch) + x;

					for (long y = y0; y < y1; ++y) {
						if (m_pTexture->GetPixel(u, (v * m_pTexture->m_Height) >> (c_FixedPointBits + 8))) {
							// Apply light-mapping to fade the color towards black.
							*pShape = shapeID;
							*pDepth = zlerp;
						}

						pShape += m_FramePitch;
						pDepth += m_FramePitch;
						v      += vd;
					}
				}
			}

			yT += yTd;
			vT += vTd;
			yB += yBd;
			vB += vBd;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintNgon()
//
void CwaRaster::PaintNgon(Vertex_t corners[], DWORD cornerCount)
{
	PROFILE(PFID_Raster_PaintNgon);

	long  minY = corners[0].y;
	DWORD minI = 0;
	for (DWORD cn = 1; cn < cornerCount; ++cn) {
		if (corners[cn].y < minY) {
			minY = corners[cn].y;
			minI = cn;
		}
	}

	DWORD iLT = minI;
	DWORD iRT = minI;
	DWORD iLB = (minI + cornerCount - 1) % cornerCount;
	DWORD iRB = (minI + 1) % cornerCount;

	long yT = (minY + c_FixedPointMask) >> c_FixedPointBits;
	long yB;
	long y  = yT;
	bool limitOnLeft;

	long zLT = FixedDiv(c_FixedPointOne, corners[iLT].z);
	long zLB = FixedDiv(c_FixedPointOne, corners[iLB].z);
	long zRT = FixedDiv(c_FixedPointOne, corners[iRT].z);
	long zRB = FixedDiv(c_FixedPointOne, corners[iRB].z);
	long uLT = FixedMul(corners[iLT].u, zLT);
	long uLB = FixedMul(corners[iLB].u, zLB);
	long uRT = FixedMul(corners[iRT].u, zRT);
	long uRB = FixedMul(corners[iRB].u, zRB);
	long vLT = FixedMul(corners[iLT].v, zLT);
	long vLB = FixedMul(corners[iLB].v, zLB);
	long vRT = FixedMul(corners[iRT].v, zRT);
	long vRB = FixedMul(corners[iRB].v, zRB);
	long lLT = FixedMul(corners[iLT].l, zLT);
	long lLB = FixedMul(corners[iLB].l, zLB);
	long lRT = FixedMul(corners[iRT].l, zRT);
	long lRB = FixedMul(corners[iRB].l, zRB);

	do {
		if (corners[iLB].y < corners[iRB].y) {
			yB = (corners[iLB].y + c_FixedPointMask) >> c_FixedPointBits;
			limitOnLeft = true;
		}
		else {
			yB = (corners[iRB].y + c_FixedPointMask) >> c_FixedPointBits;
			limitOnLeft = false;
		}

		if (yB < 0) {
			y = yB;
		}

		for ( ; y < yB; ++y) {
			if (DWORD(y) < m_ViewHeight) {
				// Fixed-point lerp multiplier for the left and right sides
				// of the triangle.  Division is expensive, but we only do it
				// once per line.
				long mulL = FixedDiv(((y << c_FixedPointBits) - corners[iLT].y), (corners[iLB].y - corners[iLT].y));
				long mulR = FixedDiv(((y << c_FixedPointBits) - corners[iRT].y), (corners[iRB].y - corners[iRT].y));

				// Use the multiplier to compute the left and right sides of
				// the triangle.
				long xL = FixedMul((corners[iLB].x - corners[iLT].x), mulL) + corners[iLT].x;
				long xR = FixedMul((corners[iRB].x - corners[iRT].x), mulR) + corners[iRT].x;

				// Bias and shift.  This converts fixed-point values into the
				// integer coords of the left and right pixels.
				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				// Clip the left and right sides to the bounds of the frame
				// buffer.

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				// Only render the line if part of it falls within the bounds
				// of the frame buffer.
				if (xl < xr) {
					long invZ = FixedMul((zLB - zLT), mulL) + zLT;

					long uL   = FixedMul((uLB - uLT), mulL) + uLT;
					long uR   = FixedMul((uRB - uRT), mulR) + uRT;

					long vL   = FixedMul((vLB - vLT), mulL) + vLT;
					long vR   = FixedMul((vRB - vRT), mulR) + vRT;

					long lL   = FixedMul((lLB - lLT), mulL) + lLT;
					long lR   = FixedMul((lRB - lRT), mulR) + lRT;

					long z    = FixedDiv(c_FixedPointOne, invZ);

					uL = FixedMul(uL, z);
					uR = FixedMul(uR, z);
					vL = FixedMul(vL, z);
					vR = FixedMul(vR, z);
					lL = FixedMul(lL, z);
					lR = FixedMul(lR, z);

					BYTE *pLine = m_pFrame + (y * m_FramePitch);

					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long u    =   (FixedMul((uR - uL), mul0) + uL) << 4;
					long uInc = (((FixedMul((uR - uL), mul1) + uL) << 4) - u) / (xr - xl);

					long v    =   (FixedMul((vR - vL), mul0) + vL) << 4;
					long vInc = (((FixedMul((vR - vL), mul1) + vL) << 4) - v) / (xr - xl);

					long l    =   FixedMul((lR - lL), mul0) + lL;
					long lInc = ((FixedMul((lR - lL), mul1) + lL) - l) / (xr - xl);
/*
					long u    = (FixedMul((uR-uL), mul0) + uL) << 4;
					long v    = (FixedMul((vR-vL), mul0) + vL) << 4;
					long l    =  FixedMul((lR-lL), mul0) + lL;
					long uInc = (FixedMul((uR-uL), (mul1-mul0)) << 4) / (xr-xl);
					long vInc = (FixedMul((vR-vL), (mul1-mul0)) << 4) / (xr-xl);
					long lInc =  FixedMul((lR-lL), (mul1-mul0))       / (xr-xl);
*/
					for (DWORD x = xl; x < DWORD(xr); ++x) {
//						DWORD u2 = (u * m_pTexture->m_Width ) >> (c_FixedPointBits + 4);
//						DWORD v2 = (v * m_pTexture->m_Height) >> (c_FixedPointBits + 4);
						DWORD u2 = (u >> (c_FixedPointBits - 6 + 4)) & 0x3F;
						DWORD v2 = (v >> (c_FixedPointBits - 6 + 4)) & 0x3F;

//						if (u2 >= m_pTexture->m_Width) {
//							u2 = m_pTexture->m_Width - 1;
//						}
//						if (v2 >= m_pTexture->m_Height) {
//							v2 = m_pTexture->m_Height - 1;
//						}

						pLine[x] = m_pLightMap[LumaIndex(l,x,y) | m_pTexture->GetPixel(u2, v2)];

						u += uInc;
						v += vInc;
						l += lInc;
					}
				}
			}
		}

		if (limitOnLeft) {
			iLT = iLB;
			iLB = (iLB + cornerCount - 1) % cornerCount;

			zLT = zLB;
			uLT = uLB;
			vLT = vLB;
			lLT = lLB;

			zLB = FixedDiv(c_FixedPointOne, corners[iLB].z);
			uLB = FixedMul(corners[iLB].u, zLB);
			vLB = FixedMul(corners[iLB].v, zLB);
			lLB = FixedMul(corners[iLB].l, zLB);
		}
		else {
			iRT = iRB;
			iRB = (iRB + 1) % cornerCount;

			zRT = zRB;
			uRT = uRB;
			vRT = vRB;
			lRT = lRB;

			zRB = FixedDiv(c_FixedPointOne, corners[iRB].z);
			uRB = FixedMul(corners[iRB].u, zRB);
			vRB = FixedMul(corners[iRB].v, zRB);
			lRB = FixedMul(corners[iRB].l, zRB);
		}
	} while ((iLT != iRT) && (y < long(m_ViewHeight)));
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintNgonFast()
//
//	Alternate version of PaintNgon().  This version does not use perspective
//	correction based on inverse Z values, so the textures have perspective
//	distortion in them.  However, the code ends up not being that much faster
//	than PaintNgon() for moderate-sized quads.  Both versions employ about
//	the same amount of effort to plot pixels, but this version has a lower
//	per-line set-up time, so is much faster if the quad is only a few pixels
//	across -- in other words, quads far away render much faster, and their
//	smaller size makes the perspective distortion less obvious, making this
//	version a good choice for distant polygons.
//
void CwaRaster::PaintNgonFast(Vertex_t corners[], DWORD cornerCount)
{
	PROFILE(PFID_Raster_PaintNgonFast);

	long  minY = corners[0].y;
	DWORD minI = 0;
	for (DWORD cn = 1; cn < cornerCount; ++cn) {
		if (corners[cn].y < minY) {
			minY = corners[cn].y;
			minI = cn;
		}
	}

	DWORD iLT = minI;
	DWORD iRT = minI;
	DWORD iLB = (minI + cornerCount - 1) % cornerCount;
	DWORD iRB = (minI + 1) % cornerCount;

	long yT = (minY + c_FixedPointMask) >> c_FixedPointBits;
	long yB;
	long y  = yT;
	bool limitOnLeft;

	do {
		if (corners[iLB].y < corners[iRB].y) {
			yB = (corners[iLB].y + c_FixedPointMask) >> c_FixedPointBits;
			limitOnLeft = true;
		}
		else {
			yB = (corners[iRB].y + c_FixedPointMask) >> c_FixedPointBits;
			limitOnLeft = false;
		}

		for ( ; y < yB; ++y) {
			if (DWORD(y) < m_ViewHeight) {
				// Fixed-point lerp multiplier for the left and right sides
				// of the triangle.  Division is expensive, but we only do it
				// once per line.
				long mulL = FixedDiv(((y << c_FixedPointBits) - corners[iLT].y), (corners[iLB].y - corners[iLT].y));
				long mulR = FixedDiv(((y << c_FixedPointBits) - corners[iRT].y), (corners[iRB].y - corners[iRT].y));

				// Use the multiplier to compute the left and right sides of
				// the triangle.
				long xL = FixedMul((corners[iLB].x - corners[iLT].x), mulL) + corners[iLT].x;
				long xR = FixedMul((corners[iRB].x - corners[iRT].x), mulR) + corners[iRT].x;

				// Bias and shift.  This converts fixed-point values into the
				// integer coords of the left and right pixels.
				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				// Clip the left and right sides to the bounds of the frame
				// buffer.

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				// Only render the line if part of it falls within the bounds
				// of the frame buffer.
				if (xl < xr) {
					long uL = FixedMul((corners[iLB].u - corners[iLT].u), mulL) + corners[iLT].u;
					long uR = FixedMul((corners[iRB].u - corners[iRT].u), mulR) + corners[iRT].u;

					long vL = FixedMul((corners[iLB].v - corners[iLT].v), mulL) + corners[iLT].v;
					long vR = FixedMul((corners[iRB].v - corners[iRT].v), mulR) + corners[iRT].v;

					long lL = FixedMul((corners[iLB].l - corners[iLT].l), mulL) + corners[iLT].l;
					long lR = FixedMul((corners[iRB].l - corners[iRT].l), mulR) + corners[iRT].l;

					BYTE *pLine = m_pFrame + (y * m_FramePitch);

					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long u    =   (FixedMul((uR - uL), mul0) + uL) << 4;
					long uInc = (((FixedMul((uR - uL), mul1) + uL) << 4) - u) / (xr - xl);

					long v    =   (FixedMul((vR - vL), mul0) + vL) << 4;
					long vInc = (((FixedMul((vR - vL), mul1) + vL) << 4) - v) / (xr - xl);

					long l    =   FixedMul((lR - lL), mul0) + lL;
					long lInc = ((FixedMul((lR - lL), mul1) + lL) - l) / (xr - xl);

					for (DWORD x = xl; x < DWORD(xr); ++x) {
//						DWORD u2 = (u * m_pTexture->m_Width ) >> (c_FixedPointBits + 4);
//						DWORD v2 = (v * m_pTexture->m_Height) >> (c_FixedPointBits + 4);
						DWORD u2 = (u >> (c_FixedPointBits - 6 + 4)) & 0x3F;
						DWORD v2 = (v >> (c_FixedPointBits - 6 + 4)) & 0x3F;

//						if (u2 >= m_pTexture->m_Width) {
//							u2 = m_pTexture->m_Width - 1;
//						}
//						if (v2 >= m_pTexture->m_Height) {
//							v2 = m_pTexture->m_Height - 1;
//						}

						pLine[x] = m_pLightMap[LumaIndex(l,x,y) | m_pTexture->GetPixel(u2, v2)];

						u += uInc;
						v += vInc;
						l += lInc;
					}
				}
			}
		}

		if (limitOnLeft) {
			iLT = iLB;
			iLB = (iLB + cornerCount - 1) % cornerCount;
		}
		else {
			iRT = iRB;
			iRB = (iRB + 1) % cornerCount;
		}
	} while (iLT != iRT);
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintSprite()
//
void CwaRaster::PaintSprite(Vertex_t corners[])
{
	PROFILE(PFID_Raster_PaintSprite);

	// Since the sides are parallel to one another, we only need to compute
	// the frame coords for each side of the rectangle once.
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[3].y + c_FixedPointMask) >> c_FixedPointBits;

	// Clamp the coords to the bounds of the frame buffer.

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	DWORD luma = corners[0].l;

	long uL = corners[0].u << 6;
	long uR = corners[1].u << 6;

	long uInc = FixedDiv((uR - uL), (corners[1].x - corners[0].x));

	uL += FixedMul(((left << c_FixedPointBits) - corners[0].x), uInc);

	for (long y = top; y < bottom; ++y) {
		if (DWORD(y) < m_ViewHeight) {
			long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[3].y - corners[0].y));

			BYTE *pLine = m_pFrame + (y * m_FramePitch);

			DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

			if (v2 >= m_pTexture->m_Height) {
				v2 = m_pTexture->m_Height - 1;
			}

			long u = uL;

			for (long x = left; x < right; ++x) {
				DWORD u2 = (u * m_pTexture->m_Width) >> (c_FixedPointBits + 6);

//				if (u2 >= m_pTexture->m_Width) {
//					u2 = m_pTexture->m_Width - 1;
//				}

				register BYTE pixel = m_pTexture->GetPixel(u2, v2);

				if (pixel) {
					pLine[x] = m_pLightMap[LumaIndex(luma,x,y) | DWORD(pixel)];
				}

				u += uInc;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	PaintSpriteTranslucent()
//
void CwaRaster::PaintSpriteTranslucent(Vertex_t corners[])
{
	PROFILE(PFID_Raster_PaintSpriteTranslucent);

	// Since the sides are parallel to one another, we only need to compute
	// the frame coords for each side of the rectangle once.
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[3].y + c_FixedPointMask) >> c_FixedPointBits;

	// Clamp the coords to the bounds of the frame buffer.

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	long uL = corners[0].u << 6;
	long uR = corners[1].u << 6;

	long uInc = FixedDiv((uR - uL), (corners[1].x - corners[0].x));

	uL += FixedMul(((left << c_FixedPointBits) - corners[0].x), uInc);

	for (long y = top; y < bottom; ++y) {
		if (DWORD(y) < m_ViewHeight) {
			long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[3].y - corners[0].y));

			BYTE *pLine = m_pFrame + (y * m_FramePitch);

			DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

			if (v2 >= m_pTexture->m_Height) {
				v2 = m_pTexture->m_Height - 1;
			}

			long u = uL;

			for (long x = left; x < right; ++x) {
				DWORD u2 = (u * m_pTexture->m_Width) >> (c_FixedPointBits + 6);

				if (u2 >= m_pTexture->m_Width) {
					u2 = m_pTexture->m_Width - 1;
				}

				register BYTE pixel = m_pTexture->GetPixel(u2, v2);

				if (pixel) {
					// Colors < 14 are really darkness values.  Read the
					// color value already in the frame buffer, darken it
					// by the amount stored in the texture, and write the
					// result back to the frame buffer.
					if (pixel < BYTE(14)) {
						pLine[x] = m_pLightMap[(DWORD(13-pixel) << 8) | DWORD(pLine[x])];
					}

					// All other values are treated as normal colors.
					// For ghosts/wraiths, these are the eyes.
					else {
						pLine[x] = pixel;
					}
				}

				u += uInc;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawTriangleSolid()
//
//	Obsolete code.  Used to for initial testing of rasterizer.  Kept only
//	for reference purposes.
//
void CwaRaster::DrawTriangleSolid(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, BYTE color)
{
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long x0 = pA->x;
	long y0 = pA->y;
	long x1 = pB->x;
	long y1 = pB->y;
	long x2 = pC->x;
	long y2 = pC->y;

	long dxr = 0;
	long dxl = 0;
	long dxb = 0;
	long yMid, yBottom;

	long yTop = (y0 + c_FixedPointMask) >> c_FixedPointBits;

	if (y1 < y2) {
		yMid    = (y1 + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (y2 + c_FixedPointMask) >> c_FixedPointBits;

		if (yMid > yTop) {
			dxr = FixedDiv((x1 - x0), (y1 - y0));
		}

		if (yBottom > yTop) {
			dxl = FixedDiv((x2 - x0), (y2 - y0));
		}

		if (yBottom > yMid) {
			dxb = FixedDiv((x1 - x2), (y2 - y1));
		}
	}
	else {
		yMid    = (y2 + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (y1 + c_FixedPointMask) >> c_FixedPointBits;

		if (yBottom > yTop) {
			dxr = FixedDiv((x1 - x0), (y1 - y0));
		}

		if (yMid > yTop) {
			dxl = FixedDiv((x2 - x0), (y2 - y0));
		}

		if (yBottom > yMid) {
			dxb = FixedDiv((x1 - x2), (y1 - y2));
		}
	}


	long xL = x0;
	long xR = x0;
	long y  = yTop;
	long yStop = yMid;
	DWORD repeat = 2;

	do {
		for ( ; y < yStop; ++y) {
			if (DWORD(y) < m_ViewHeight) {
				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				BYTE *pLine = m_pFrame + (y * m_FramePitch);

				for (long x = xl; x < xr; ++x) {
					pLine[x] = color;
				}
			}

			xL += dxl;
			xR += dxr;
		}

		if (y1 < y2) {
			dxr = -dxb;
			xR  = x1;
		}
		else {
			dxl = dxb;
			xL  = x2;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Scanline interpolation requires lerping multiple values along the length
//	of a scanline.  For triangle rasterization, this means two sets of values
//	are required: one for the top half of the triangle, and one for the bottom.
//	The only exceptions are triangles with a perfectly horizontal top or
//	bottom.  To manage this info, two ScanLine_t structs are kept, one for the
//	left side and one for the right side.
//
//	Initial values are stored in x0 (or y0, z1, etc.).  The code will lerp
//	from x0 to x1, then stop.  This will be the split point between the top
//	and bottom halves of the triangle.  The final value of x is stored in
//	xSplit. When the code reaches the split point, it resets x0 = x1, and
//	x1 = xSplit, then resumes processing.  This is somewhat cumbersome to
//	deal with, but allows the code to render the two halves of a triangle
//	within a loop, rather than duplicating the rasterization code for each
//	half of the triangle.
//
struct ScanLine_t
{
	long x0;
	long x1;
	long xSplit;
	long y0;
	long y1;
	long ySplit;
	long z0;
	long z1;
	long zSplit;
	long u0;
	long u1;
	long uSplit;
	long v0;
	long v1;
	long vSplit;
	long l0;
	long l1;
	long lSplit;
};


/////////////////////////////////////////////////////////////////////////////
//
//	DrawTriangleTest()
//
//	This is the basic triangle rasterization code.  All of the other triangle
//	functions work exactly the same as far as set-up and looping.  The
//	differences are in the inner loops, regarding how values are interpolated
//	across lines.
//
//	This version does all interpolation in 1/z space.  This makes all of the
//	interpolation occur in non-linear space, which is required for perspective
//	correct rendering.  It's also much more expensive due to all of the
//	divisions required, but is needed to avoid the horrible shearing artifacts
//	that would otherwise occur.
//
void CwaRaster::DrawTriangleTest(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC)
{
	// Assume clock-wise widing of the vertices.  Do a simple sort operation
	// so that A is the top-most vertex.  Whether B or C is the bottom-most
	// vertex is what determines how the triangle is split into top and
	// bottom halves.
	//
	// Note that no special case is made for back-facing triangles.  Those
	// should have been filtered out at a higher level.  This code will plow
	// through all of the work to set up rendering them, but the end result
	// is no pixels will be plotted if the triangle is back-facing.  So
	// it is must cheaper to never give DrawTriangle() a back-facing triangle.
	//
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long yMid, yBottom;

	// Map the y-coord of the top vertex to a line in the frame buffer.
	long yTop = (pA->y + c_FixedPointMask) >> c_FixedPointBits;

	ScanLine_t left, right;

	// When splitting the triangle into top and bottom halves, which side
	// is the long side?  We can interpolate continuously along the long
	// side, but the short side has to be reset when we hit the vertex.
	bool splitLeft;

	// If B < C, then AC is the long side, and AB is the short side.  That
	// means the left side is long, so the split is on the right side.
	// Rendering will proceed down to the line that B is on, reset the
	// right side of the triangle, then resume interpolating.
	if (pB->y < pC->y) {
		yMid    = (pB->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pC->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = false;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);		// interpolate 1/z
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.u0     = FixedMul(pA->u, left.z0);				// interpolate u/z
		left.u1     = FixedMul(pC->u, left.z1);
		left.v0     = FixedMul(pA->v, left.z0);				// interpolate v/z
		left.v1     = FixedMul(pC->v, left.z1);
		left.l0     = pA->l;
		left.l1     = pC->l;

		// These are not used, but needed to eliminate stupid compiler warnings.
		left.xSplit = 0;
		left.ySplit = 0;
		left.zSplit = 0;
		left.uSplit = 0;
		left.vSplit = 0;
		left.lSplit = 0;

		right.x0     = pA->x;
		right.x1     = pB->x;
		right.xSplit = pC->x;
		right.y0     = pA->y;
		right.y1     = pB->y;
		right.ySplit = pC->y;
		right.z0     = FixedDiv(c_FixedPointOne, pA->z);
		right.z1     = FixedDiv(c_FixedPointOne, pB->z);
		right.zSplit = FixedDiv(c_FixedPointOne, pC->z);
		right.u0     = FixedMul(pA->u, right.z0 );
		right.u1     = FixedMul(pB->u, right.z1);
		right.uSplit = FixedMul(pC->u, right.zSplit);
		right.v0     = FixedMul(pA->v, right.z0 );
		right.v1     = FixedMul(pB->v, right.z1);
		right.vSplit = FixedMul(pC->v, right.zSplit);
		right.l0     = pA->l;
		right.l1     = pB->l;
		right.lSplit = pC->l;
	}

	// Otherwise C comes before B, so AB is the long side.  That means the
	// split is on the left, so we proceed down the triangle to the line
	// that C is on, reset the interpolation values for the left side,
	// then resume interpolating the bottom half of the triangle.
	else {
		yMid    = (pC->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pB->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = true;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.xSplit = pB->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.ySplit = pB->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.zSplit = FixedDiv(c_FixedPointOne, pB->z);
		left.u0     = FixedMul(pA->u, left.z0);
		left.u1     = FixedMul(pC->u, left.z1);
		left.uSplit = FixedMul(pB->u, left.zSplit);
		left.v0     = FixedMul(pA->v, left.z0);
		left.v1     = FixedMul(pC->v, left.z1);
		left.vSplit = FixedMul(pB->v, left.zSplit);
		left.l0     = pA->l;
		left.l1     = pC->l;
		left.lSplit = pB->l;

		right.x0    = pA->x;
		right.x1    = pB->x;
		right.y0    = pA->y;
		right.y1    = pB->y;
		right.z0    = FixedDiv(c_FixedPointOne, pA->z);
		right.z1    = FixedDiv(c_FixedPointOne, pB->z);
		right.u0    = FixedMul(pA->u, right.z0);
		right.u1    = FixedMul(pB->u, right.z1);
		right.v0    = FixedMul(pA->v, right.z0);
		right.v1    = FixedMul(pB->v, right.z1);
		right.l0    = pA->l;
		right.l1    = pB->l;

		// These are not used, but needed to eliminate stupid compiler warnings.
		right.xSplit = 0;
		right.ySplit = 0;
		right.zSplit = 0;
		right.uSplit = 0;
		right.vSplit = 0;
		right.lSplit = 0;
	}

	// Start with the top line of the triangle.  Stop when we get to the middle
	// line of the triangle (the one that has B or C, whichever comes first).
	long  y      = yTop;
	long  yStop  = yMid;
	DWORD repeat = 2;

	// Loop twice, once for the top half, and once for the bottom.  If the
	// top or bottom of the triangle is flat, then either yTop == yMid, or
	// yMid == yBottom, making that loop into a no-op.
	//
	do {

		// Keep looping until we hit the bottom line for this half of the
		// triangle.
		for ( ; y < yStop; ++y) {

			// Skip lines outside the renderable portion of the frame buffer.
			// This is an unsigned comparison, so negative lines are treated
			// as being really large positive values that definitely fall
			// outside the frame buffer (it also removes the need for both a
			// > and < test, which is a minor optimization).
			if (DWORD(y) < m_ViewHeight) {

				// Fixed-point lerp multiplier for the left and right sides
				// of the triangle.  Division is expensive, but we only do it
				// once per line.
				long mulL = FixedDiv(((y << c_FixedPointBits) - left.y0),  (left.y1  - left.y0));
				long mulR = FixedDiv(((y << c_FixedPointBits) - right.y0), (right.y1 - right.y0));

				// Use the multiplier to compute the left and right sides of
				// the triangle.
				long xL = FixedMul((left.x1  - left.x0),  mulL) + left.x0;
				long xR = FixedMul((right.x1 - right.x0), mulR) + right.x0;

				// Bias and shift.  This converts fixed-point values into the
				// integer coords of the left and right pixels.
				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				// Clip the left and right sides to the bounds of the frame
				// buffer.

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				// Only render the line if part of it falls within the bounds
				// of the frame buffer.
				if (xl < xr) {

					// LERP along the side of the triangle to compute the
					// initial and final values for each value.
					long zL = FixedMul((left.z1  - left.z0),  mulL) + left.z0;
					long zR = FixedMul((right.z1 - right.z0), mulR) + right.z0;

					long uL = FixedMul((left.u1  - left.u0),  mulL) + left.u0;
					long uR = FixedMul((right.u1 - right.u0), mulR) + right.u0;

					long vL = FixedMul((left.v1  - left.v0),  mulL) + left.v0;
					long vR = FixedMul((right.v1 - right.v0), mulR) + right.v0;

					long lL = FixedMul((left.l1  - left.l0),  mulL) + left.l0;
					long lR = FixedMul((right.l1 - right.l0), mulR) + right.l0;

					BYTE  *pLine = m_pFrame + (y * m_FramePitch);

					// Compute the multipliers needed to set up the initial and
					// final values for each line.  This is needed since a line
					// may have been cropped to the sides of the frame buffer.
					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long ender;

					// Compute the initial value, and the amount to increment
					// for each pixel.  All of these interpolations are occurring
					// in 1/z space (the "z" variable is actually 1/z).

					long z    = (FixedMul((zR - zL), mul0) + zL) << c_ZDepthBias;
					ender     = (FixedMul((zR - zL), mul1) + zL) << c_ZDepthBias;
					long zInc = (ender - z) / (xr - xl);

					long u    = (FixedMul((uR - uL), mul0) + uL) << c_ZDepthBias;
					ender     = (FixedMul((uR - uL), mul1) + uL) << c_ZDepthBias;
					long uInc = (ender - u) / (xr - xl);

					long v    = (FixedMul((vR - vL), mul0) + vL) << c_ZDepthBias;
					ender     = (FixedMul((vR - vL), mul1) + vL) << c_ZDepthBias;
					long vInc = (ender - v) / (xr - xl);

					long l    = FixedMul((lR - lL), mul0) + lL;
					ender     = FixedMul((lR - lL), mul1) + lL;
					long lInc = (ender - l) / (xr - xl);

					// Render all of the pixels on the line.  Since xl and xr
					// were cropped to the bounds of the frame buffer, no per-pixel
					// bounds tests are required.
					for (long x = xl; x < xr; ++x) {

#if 0
						// This is the accurate-but-expensive perspective division
						// required to factor the 1/z out of u/z, which converts
						// u back into linear space.
						long uTemp = FixedDiv(u, z);
						long vTemp = FixedDiv(v, z);
#else
						// Alternate implementation.  This uses a look-up table
						// filled with pre-computed 1/z values so it can multiply
						// instead of divide.  This can be up to 30% faster,
						// depending upon the level of overdraw in rendering.
						long uTemp = FastInvZ(u, z);
						long vTemp = FastInvZ(v, z);
#endif

							// Convert the unit u/v values into texture coordinates.
//						DWORD u2 = ((uTemp * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//						DWORD v2 = ((vTemp * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
						DWORD u2 = (uTemp * m_pTexture->m_Width ) >> c_FixedPointBits;
						DWORD v2 = (vTemp * m_pTexture->m_Height) >> c_FixedPointBits;

						// Clamp to the bounds of the texture buffer.
						if (u2 >= m_pTexture->m_Width) {
							u2 = m_pTexture->m_Width - 1;
						}
						if (v2 >= m_pTexture->m_Height) {
							v2 = m_pTexture->m_Height - 1;
						}

						// Fetch the color-mapped value for this texture coord.
						DWORD pixel = m_pTexture->GetPixel(u2, v2);

						// Apply light-mapping to fade the color towards black.
						pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

						z += zInc;
						u += uInc;
						v += vInc;
						l += lInc;
					}
				}
			}
		}

		if (splitLeft) {
			left.x0 = left.x1;
			left.x1 = left.xSplit;
			left.y0 = left.y1;
			left.y1 = left.ySplit;
			left.z0 = left.z1;
			left.z1 = left.zSplit;
			left.u0 = left.u1;
			left.u1 = left.uSplit;
			left.v0 = left.v1;
			left.v1 = left.vSplit;
			left.l0 = left.l1;
			left.l1 = left.lSplit;
		}
		else {
			right.x0 = right.x1;
			right.x1 = right.xSplit;
			right.y0 = right.y1;
			right.y1 = right.ySplit;
			right.z0 = right.z1;
			right.z1 = right.zSplit;
			right.u0 = right.u1;
			right.u1 = right.uSplit;
			right.v0 = right.v1;
			right.v1 = right.vSplit;
			right.l0 = right.l1;
			right.l1 = right.lSplit;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


void CwaRaster::DrawTriangleTestPer(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC)
{
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long yMid, yBottom;

	long yTop = (pA->y + c_FixedPointMask) >> c_FixedPointBits;

	ScanLine_t left, right;

	bool splitLeft;

	if (pB->y < pC->y) {
		yMid    = (pB->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pC->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = false;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.u0     = pA->u;
		left.u1     = pC->u;
		left.v0     = pA->v;
		left.v1     = pC->v;
		left.l0     = pA->l;
		left.l1     = pC->l;

		left.xSplit = 0;
		left.ySplit = 0;
		left.zSplit = 0;
		left.uSplit = 0;
		left.vSplit = 0;
		left.lSplit = 0;

		right.x0     = pA->x;
		right.x1     = pB->x;
		right.xSplit = pC->x;
		right.y0     = pA->y;
		right.y1     = pB->y;
		right.ySplit = pC->y;
		right.z0     = FixedDiv(c_FixedPointOne, pA->z);
		right.z1     = FixedDiv(c_FixedPointOne, pB->z);
		right.zSplit = FixedDiv(c_FixedPointOne, pC->z);
		right.u0     = pA->u;
		right.u1     = pB->u;
		right.uSplit = pC->u;
		right.v0     = pA->v;
		right.v1     = pB->v;
		right.vSplit = pC->v;
		right.l0     = pA->l;
		right.l1     = pB->l;
		right.lSplit = pC->l;
	}
	else {
		yMid    = (pC->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pB->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = true;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.xSplit = pB->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.ySplit = pB->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.zSplit = FixedDiv(c_FixedPointOne, pB->z);
		left.u0     = pA->u;
		left.u1     = pC->u;
		left.uSplit = pB->u;
		left.v0     = pA->v;
		left.v1     = pC->v;
		left.vSplit = pB->v;
		left.l0     = pA->l;
		left.l1     = pC->l;
		left.lSplit = pB->l;

		right.x0    = pA->x;
		right.x1    = pB->x;
		right.y0    = pA->y;
		right.y1    = pB->y;
		right.z0    = FixedDiv(c_FixedPointOne, pA->z);
		right.z1    = FixedDiv(c_FixedPointOne, pB->z);
		right.u0    = pA->u;
		right.u1    = pB->u;
		right.v0    = pA->v;
		right.v1    = pB->v;
		right.l0    = pA->l;
		right.l1    = pB->l;

		right.xSplit = 0;
		right.ySplit = 0;
		right.zSplit = 0;
		right.uSplit = 0;
		right.vSplit = 0;
		right.lSplit = 0;
	}

	long  y      = yTop;
	long  yStop  = yMid;
	DWORD repeat = 2;
	
	do {
		for ( ; y < yStop; ++y) {

			if (DWORD(y) < m_ViewHeight) {
				long mulL = FixedDiv(((y << c_FixedPointBits) - left.y0),  (left.y1  - left.y0));
				long mulR = FixedDiv(((y << c_FixedPointBits) - right.y0), (right.y1 - right.y0));

				long xL = FixedMul((left.x1  - left.x0),  mulL) + left.x0;
				long xR = FixedMul((right.x1 - right.x0), mulR) + right.x0;

				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				if (xl < xr) {
					long zL = FixedMul((left.z1  - left.z0),  mulL) + left.z0;
					long zR = FixedMul((right.z1 - right.z0), mulR) + right.z0;

					long uL = FixedMul((left.u1  - left.u0),  mulL) + left.u0;
					long uR = FixedMul((right.u1 - right.u0), mulR) + right.u0;

					long vL = FixedMul((left.v1  - left.v0),  mulL) + left.v0;
					long vR = FixedMul((right.v1 - right.v0), mulR) + right.v0;

					long lL = FixedMul((left.l1  - left.l0),  mulL) + left.l0;
					long lR = FixedMul((right.l1 - right.l0), mulR) + right.l0;

					BYTE *pLine = m_pFrame + (y * m_FramePitch);

					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long ender;

					long z    = (FixedMul((zR - zL), mul0) + zL) << c_ZDepthBias;
					ender     = (FixedMul((zR - zL), mul1) + zL) << c_ZDepthBias;
					long zInc = (ender - z) / (xr - xl);

					long u    = FixedMul((uR - uL), mul0) + uL;
					ender     = FixedMul((uR - uL), mul1) + uL;
					long uInc = (ender - u) / (xr - xl);

					long v    = FixedMul((vR - vL), mul0) + vL;
					ender     = FixedMul((vR - vL), mul1) + vL;
					long vInc = (ender - v) / (xr - xl);

					long l    = FixedMul((lR - lL), mul0) + lL;
					ender     = FixedMul((lR - lL), mul1) + lL;
					long lInc = (ender - l) / (xr - xl);

					for (long x = xl; x < xr; ++x) {
//						DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//						DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
						DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
						DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

						if (u2 >= m_pTexture->m_Width) {
							u2 = m_pTexture->m_Width - 1;
						}
						if (v2 >= m_pTexture->m_Height) {
							v2 = m_pTexture->m_Height - 1;
						}

						DWORD pixel = m_pTexture->GetPixel(u2, v2);

						pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

						z += zInc;
						u += uInc;
						v += vInc;
						l += lInc;
					}
				}
			}
		}

		if (splitLeft) {
			left.x0 = left.x1;
			left.x1 = left.xSplit;
			left.y0 = left.y1;
			left.y1 = left.ySplit;
			left.z0 = left.z1;
			left.z1 = left.zSplit;
			left.u0 = left.u1;
			left.u1 = left.uSplit;
			left.v0 = left.v1;
			left.v1 = left.vSplit;
			left.l0 = left.l1;
			left.l1 = left.lSplit;
		}
		else {
			right.x0 = right.x1;
			right.x1 = right.xSplit;
			right.y0 = right.y1;
			right.y1 = right.ySplit;
			right.z0 = right.z1;
			right.z1 = right.zSplit;
			right.u0 = right.u1;
			right.u1 = right.uSplit;
			right.v0 = right.v1;
			right.v1 = right.vSplit;
			right.l0 = right.l1;
			right.l1 = right.lSplit;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawTriangle()
//
//	This is the basic triangle rasterization code.  All of the other triangle
//	functions work exactly the same as far as set-up and looping.  The
//	differences are in the inner loops, regarding how values are interpolated
//	across lines.
//
//	This version does all interpolation in 1/z space.  This makes all of the
//	interpolation occur in non-linear space, which is required for perspective
//	correct rendering.  It's also much more expensive due to all of the
//	divisions required, but is needed to avoid the horrible shearing artifacts
//	that would otherwise occur.
//
void CwaRaster::DrawTriangle(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID)
{
	// Assume clock-wise widing of the vertices.  Do a simple sort operation
	// so that A is the top-most vertex.  Whether B or C is the bottom-most
	// vertex is what determines how the triangle is split into top and
	// bottom halves.
	//
	// Note that no special case is made for back-facing triangles.  Those
	// should have been filtered out at a higher level.  This code will plow
	// through all of the work to set up rendering them, but the end result
	// is no pixels will be plotted if the triangle is back-facing.  So
	// it is must cheaper to never give DrawTriangle() a back-facing triangle.
	//
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long yMid, yBottom;

	// Map the y-coord of the top vertex to a line in the frame buffer.
	long yTop = (pA->y + c_FixedPointMask) >> c_FixedPointBits;

	ScanLine_t left, right;

	// When splitting the triangle into top and bottom halves, which side
	// is the long side?  We can interpolate continuously along the long
	// side, but the short side has to be reset when we hit the vertex.
	bool splitLeft;

	// If B < C, then AC is the long side, and AB is the short side.  That
	// means the left side is long, so the split is on the right side.
	// Rendering will proceed down to the line that B is on, reset the
	// right side of the triangle, then resume interpolating.
	if (pB->y < pC->y) {
		yMid    = (pB->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pC->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = false;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);		// interpolate 1/z
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.u0     = FixedMul(pA->u, left.z0);				// interpolate u/z
		left.u1     = FixedMul(pC->u, left.z1);
		left.v0     = FixedMul(pA->v, left.z0);				// interpolate v/z
		left.v1     = FixedMul(pC->v, left.z1);
		left.l0     = pA->l;
		left.l1     = pC->l;

		// These are not used, but needed to eliminate stupid compiler warnings.
		left.xSplit = 0;
		left.ySplit = 0;
		left.zSplit = 0;
		left.uSplit = 0;
		left.vSplit = 0;
		left.lSplit = 0;

		right.x0     = pA->x;
		right.x1     = pB->x;
		right.xSplit = pC->x;
		right.y0     = pA->y;
		right.y1     = pB->y;
		right.ySplit = pC->y;
		right.z0     = FixedDiv(c_FixedPointOne, pA->z);
		right.z1     = FixedDiv(c_FixedPointOne, pB->z);
		right.zSplit = FixedDiv(c_FixedPointOne, pC->z);
		right.u0     = FixedMul(pA->u, right.z0 );
		right.u1     = FixedMul(pB->u, right.z1);
		right.uSplit = FixedMul(pC->u, right.zSplit);
		right.v0     = FixedMul(pA->v, right.z0 );
		right.v1     = FixedMul(pB->v, right.z1);
		right.vSplit = FixedMul(pC->v, right.zSplit);
		right.l0     = pA->l;
		right.l1     = pB->l;
		right.lSplit = pC->l;
	}

	// Otherwise C comes before B, so AB is the long side.  That means the
	// split is on the left, so we proceed down the triangle to the line
	// that C is on, reset the interpolation values for the left side,
	// then resume interpolating the bottom half of the triangle.
	else {
		yMid    = (pC->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pB->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = true;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.xSplit = pB->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.ySplit = pB->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.zSplit = FixedDiv(c_FixedPointOne, pB->z);
		left.u0     = FixedMul(pA->u, left.z0);
		left.u1     = FixedMul(pC->u, left.z1);
		left.uSplit = FixedMul(pB->u, left.zSplit);
		left.v0     = FixedMul(pA->v, left.z0);
		left.v1     = FixedMul(pC->v, left.z1);
		left.vSplit = FixedMul(pB->v, left.zSplit);
		left.l0     = pA->l;
		left.l1     = pC->l;
		left.lSplit = pB->l;

		right.x0    = pA->x;
		right.x1    = pB->x;
		right.y0    = pA->y;
		right.y1    = pB->y;
		right.z0    = FixedDiv(c_FixedPointOne, pA->z);
		right.z1    = FixedDiv(c_FixedPointOne, pB->z);
		right.u0    = FixedMul(pA->u, right.z0);
		right.u1    = FixedMul(pB->u, right.z1);
		right.v0    = FixedMul(pA->v, right.z0);
		right.v1    = FixedMul(pB->v, right.z1);
		right.l0    = pA->l;
		right.l1    = pB->l;

		// These are not used, but needed to eliminate stupid compiler warnings.
		right.xSplit = 0;
		right.ySplit = 0;
		right.zSplit = 0;
		right.uSplit = 0;
		right.vSplit = 0;
		right.lSplit = 0;
	}

	// Start with the top line of the triangle.  Stop when we get to the middle
	// line of the triangle (the one that has B or C, whichever comes first).
	long  y      = yTop;
	long  yStop  = yMid;
	DWORD repeat = 2;

	// Loop twice, once for the top half, and once for the bottom.  If the
	// top or bottom of the triangle is flat, then either yTop == yMid, or
	// yMid == yBottom, making that loop into a no-op.
	//
	do {

		// Keep looping until we hit the bottom line for this half of the
		// triangle.
		for ( ; y < yStop; ++y) {

			// Skip lines outside the renderable portion of the frame buffer.
			// This is an unsigned comparison, so negative lines are treated
			// as being really large positive values that definitely fall
			// outside the frame buffer (it also removes the need for both a
			// > and < test, which is a minor optimization).
			if (DWORD(y) < m_ViewHeight) {

				// Fixed-point lerp multiplier for the left and right sides
				// of the triangle.  Division is expensive, but we only do it
				// once per line.
				long mulL = FixedDiv(((y << c_FixedPointBits) - left.y0),  (left.y1  - left.y0));
				long mulR = FixedDiv(((y << c_FixedPointBits) - right.y0), (right.y1 - right.y0));

				// Use the multiplier to compute the left and right sides of
				// the triangle.
				long xL = FixedMul((left.x1  - left.x0),  mulL) + left.x0;
				long xR = FixedMul((right.x1 - right.x0), mulR) + right.x0;

				// Bias and shift.  This converts fixed-point values into the
				// integer coords of the left and right pixels.
				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				// Clip the left and right sides to the bounds of the frame
				// buffer.

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				// Only render the line if part of it falls within the bounds
				// of the frame buffer.
				if (xl < xr) {

					// LERP along the side of the triangle to compute the
					// initial and final values for each value.
					long zL = FixedMul((left.z1  - left.z0),  mulL) + left.z0;
					long zR = FixedMul((right.z1 - right.z0), mulR) + right.z0;

					long uL = FixedMul((left.u1  - left.u0),  mulL) + left.u0;
					long uR = FixedMul((right.u1 - right.u0), mulR) + right.u0;

					long vL = FixedMul((left.v1  - left.v0),  mulL) + left.v0;
					long vR = FixedMul((right.v1 - right.v0), mulR) + right.v0;

					long lL = FixedMul((left.l1  - left.l0),  mulL) + left.l0;
					long lR = FixedMul((right.l1 - right.l0), mulR) + right.l0;

					BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
					DWORD *pShape = m_pShapeID + (y * m_FramePitch);
					long  *pDepth = m_pDepth   + (y * m_FramePitch);

#if 1
					// Compute the multipliers needed to set up the initial and
					// final values for each line.  This is needed since a line
					// may have been cropped to the sides of the frame buffer.
					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long ender;

					// Compute the initial value, and the amount to increment
					// for each pixel.  All of these interpolations are occurring
					// in 1/z space (the "z" variable is actually 1/z).

					long z    = (FixedMul((zR - zL), mul0) + zL) << c_ZDepthBias;
					ender     = (FixedMul((zR - zL), mul1) + zL) << c_ZDepthBias;
					long zInc = (ender - z) / (xr - xl);

					long u    = (FixedMul((uR - uL), mul0) + uL) << c_ZDepthBias;
					ender     = (FixedMul((uR - uL), mul1) + uL) << c_ZDepthBias;
					long uInc = (ender - u) / (xr - xl);

					long v    = (FixedMul((vR - vL), mul0) + vL) << c_ZDepthBias;
					ender     = (FixedMul((vR - vL), mul1) + vL) << c_ZDepthBias;
					long vInc = (ender - v) / (xr - xl);

					long l    = FixedMul((lR - lL), mul0) + lL;
					ender     = FixedMul((lR - lL), mul1) + lL;
					long lInc = (ender - l) / (xr - xl);

					// Render all of the pixels on the line.  Since xl and xr
					// were cropped to the bounds of the frame buffer, no per-pixel
					// bounds tests are required.
					for (long x = xl; x < xr; ++x) {

						// Is this pixel closer than the one already stored in the
						// frame buffer?  Since z is really 1/z, bigger values
						// indicate closer pixels.
						if (pDepth[x] < z) {
							pDepth[x] = z;

#if 0
							// This is the accurate-but-expensive perspective division
							// required to factor the 1/z out of u/z, which converts
							// u back into linear space.
							long uTemp = FixedDiv(u, z);
							long vTemp = FixedDiv(v, z);
#else
							// Alternate implementation.  This uses a look-up table
							// filled with pre-computed 1/z values so it can multiply
							// instead of divide.  This can be up to 30% faster,
							// depending upon the level of overdraw in rendering.
							long uTemp = FastInvZ(u, z);
							long vTemp = FastInvZ(v, z);
#endif

							// Convert the unit u/v values into texture coordinates.
//							DWORD u2 = ((uTemp * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//							DWORD v2 = ((vTemp * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
							DWORD u2 = (uTemp * m_pTexture->m_Width ) >> c_FixedPointBits;
							DWORD v2 = (vTemp * m_pTexture->m_Height) >> c_FixedPointBits;

							// Clamp to the bounds of the texture buffer.
							if (u2 >= m_pTexture->m_Width) {
								u2 = m_pTexture->m_Width - 1;
							}
							if (v2 >= m_pTexture->m_Height) {
								v2 = m_pTexture->m_Height - 1;
							}

							// Fetch the color-mapped value for this texture coord.
							DWORD pixel = m_pTexture->GetPixel(u2, v2);

							// Apply light-mapping to fade the color towards black.
							pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

							// Write the ID value for this shape to the shape buffer.
							// This will allow simple look-ups to figure that what
							// object is at that location when doing picking operations.
							if (shapeID) {
								pShape[x] = shapeID;
							}
						}

						z += zInc;
						u += uInc;
						v += vInc;
						l += lInc;
					}
#else

					// Original implementation.  Brute-force apply perspective
					// division to every single pixel rendered.  This is very
					// accurate, but also expensive.  This version is kept for
					// reference purposes, but can take up to twice as long to
					// render a triangle (depending upon triangle size).

					for (long x = xl; x < xr; ++x) {
						long mul = FixedDiv(((x << c_FixedPointBits) - xL), (xR - xL));

						long z = (FixedMul((zR - zL), mul) + zL) << c_ZDepthBias;

						if (pDepth[x] < z) {
							pDepth[x] = z;

							long u = (FixedMul((uR - uL), mul) + uL) << c_ZDepthBias;
							long v = (FixedMul((vR - vL), mul) + vL) << c_ZDepthBias;

							u = FixedDiv(u, z);
							v = FixedDiv(v, z);

							long l = FixedMul((lR - lL), mul) + lL;

							DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
							DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;

							if (u2 >= m_pTexture->m_Width) {
								u2 = m_pTexture->m_Width - 1;
							}
							if (v2 >= m_pTexture->m_Height) {
								v2 = m_pTexture->m_Height - 1;
							}


							DWORD pixel = m_pTexture->GetPixel(u2, v2);

							pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

							if (shapeID) {
								pShape[x] = shapeID;
							}
						}
					}
#endif
				}
			}
		}

		if (splitLeft) {
			left.x0 = left.x1;
			left.x1 = left.xSplit;
			left.y0 = left.y1;
			left.y1 = left.ySplit;
			left.z0 = left.z1;
			left.z1 = left.zSplit;
			left.u0 = left.u1;
			left.u1 = left.uSplit;
			left.v0 = left.v1;
			left.v1 = left.vSplit;
			left.l0 = left.l1;
			left.l1 = left.lSplit;
		}
		else {
			right.x0 = right.x1;
			right.x1 = right.xSplit;
			right.y0 = right.y1;
			right.y1 = right.ySplit;
			right.z0 = right.z1;
			right.z1 = right.zSplit;
			right.u0 = right.u1;
			right.u1 = right.uSplit;
			right.v0 = right.v1;
			right.v1 = right.vSplit;
			right.l0 = right.l1;
			right.l1 = right.lSplit;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawTriangleTransparent()
//
//	This is exactly the same as DrawTriangle().  The only difference is that
//	index 0 (black) is transparent, and will not be drawn to the frame buffer.
//	This is mostly used for transparent architecture (doors, for the most
//	part).  Transparent sprites use different rendering code.
//
void CwaRaster::DrawTriangleTransparent(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID)
{
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long yMid, yBottom;

	long yTop = (pA->y + c_FixedPointMask) >> c_FixedPointBits;

	ScanLine_t left, right;

	bool splitLeft;

	if (pB->y < pC->y) {
		yMid    = (pB->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pC->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = false;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.u0     = FixedMul(pA->u, left.z0);
		left.u1     = FixedMul(pC->u, left.z1);
		left.v0     = FixedMul(pA->v, left.z0);
		left.v1     = FixedMul(pC->v, left.z1);
		left.l0     = pA->l;
		left.l1     = pC->l;

		left.xSplit = 0;
		left.ySplit = 0;
		left.zSplit = 0;
		left.uSplit = 0;
		left.vSplit = 0;
		left.lSplit = 0;

		right.x0     = pA->x;
		right.x1     = pB->x;
		right.xSplit = pC->x;
		right.y0     = pA->y;
		right.y1     = pB->y;
		right.ySplit = pC->y;
		right.z0     = FixedDiv(c_FixedPointOne, pA->z);
		right.z1     = FixedDiv(c_FixedPointOne, pB->z);
		right.zSplit = FixedDiv(c_FixedPointOne, pC->z);
		right.u0     = FixedMul(pA->u, right.z0 );
		right.u1     = FixedMul(pB->u, right.z1);
		right.uSplit = FixedMul(pC->u, right.zSplit);
		right.v0     = FixedMul(pA->v, right.z0 );
		right.v1     = FixedMul(pB->v, right.z1);
		right.vSplit = FixedMul(pC->v, right.zSplit);
		right.l0     = pA->l;
		right.l1     = pB->l;
		right.lSplit = pC->l;
	}
	else {
		yMid    = (pC->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pB->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = true;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.xSplit = pB->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.ySplit = pB->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.zSplit = FixedDiv(c_FixedPointOne, pB->z);
		left.u0     = FixedMul(pA->u, left.z0);
		left.u1     = FixedMul(pC->u, left.z1);
		left.uSplit = FixedMul(pB->u, left.zSplit);
		left.v0     = FixedMul(pA->v, left.z0);
		left.v1     = FixedMul(pC->v, left.z1);
		left.vSplit = FixedMul(pB->v, left.zSplit);
		left.l0     = pA->l;
		left.l1     = pC->l;
		left.lSplit = pB->l;

		right.x0    = pA->x;
		right.x1    = pB->x;
		right.y0    = pA->y;
		right.y1    = pB->y;
		right.z0    = FixedDiv(c_FixedPointOne, pA->z);
		right.z1    = FixedDiv(c_FixedPointOne, pB->z);
		right.u0    = FixedMul(pA->u, right.z0);
		right.u1    = FixedMul(pB->u, right.z1);
		right.v0    = FixedMul(pA->v, right.z0);
		right.v1    = FixedMul(pB->v, right.z1);
		right.l0    = pA->l;
		right.l1    = pB->l;

		right.xSplit = 0;
		right.ySplit = 0;
		right.zSplit = 0;
		right.uSplit = 0;
		right.vSplit = 0;
		right.lSplit = 0;
	}

	long  y      = yTop;
	long  yStop  = yMid;
	DWORD repeat = 2;
	
	do {
		for ( ; y < yStop; ++y) {

			if (DWORD(y) < m_ViewHeight) {
				long mulL = FixedDiv(((y << c_FixedPointBits) - left.y0),  (left.y1  - left.y0));
				long mulR = FixedDiv(((y << c_FixedPointBits) - right.y0), (right.y1 - right.y0));

				long xL = FixedMul((left.x1  - left.x0),  mulL) + left.x0;
				long xR = FixedMul((right.x1 - right.x0), mulR) + right.x0;

				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				if (xl < xr) {
					long zL = FixedMul((left.z1  - left.z0),  mulL) + left.z0;
					long zR = FixedMul((right.z1 - right.z0), mulR) + right.z0;

					long uL = FixedMul((left.u1  - left.u0),  mulL) + left.u0;
					long uR = FixedMul((right.u1 - right.u0), mulR) + right.u0;

					long vL = FixedMul((left.v1  - left.v0),  mulL) + left.v0;
					long vR = FixedMul((right.v1 - right.v0), mulR) + right.v0;

					long lL = FixedMul((left.l1  - left.l0),  mulL) + left.l0;
					long lR = FixedMul((right.l1 - right.l0), mulR) + right.l0;

					BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
					DWORD *pShape = m_pShapeID + (y * m_FramePitch);
					long  *pDepth = m_pDepth   + (y * m_FramePitch);

#if 1
					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long ender;

					long z    = (FixedMul((zR - zL), mul0) + zL) << c_ZDepthBias;
					ender     = (FixedMul((zR - zL), mul1) + zL) << c_ZDepthBias;
					long zInc = (ender - z) / (xr - xl);

					long u    = (FixedMul((uR - uL), mul0) + uL) << c_ZDepthBias;
					ender     = (FixedMul((uR - uL), mul1) + uL) << c_ZDepthBias;
					long uInc = (ender - u) / (xr - xl);

					long v    = (FixedMul((vR - vL), mul0) + vL) << c_ZDepthBias;
					ender     = (FixedMul((vR - vL), mul1) + vL) << c_ZDepthBias;
					long vInc = (ender - v) / (xr - xl);

					long l    = FixedMul((lR - lL), mul0) + lL;
					ender     = FixedMul((lR - lL), mul1) + lL;
					long lInc = (ender - l) / (xr - xl);

					for (long x = xl; x < xr; ++x) {
						if (pDepth[x] < z) {
//							long uTemp = FixedDiv(u, z);
//							long vTemp = FixedDiv(v, z);
							long uTemp = FastInvZ(u, z);
							long vTemp = FastInvZ(v, z);

//							DWORD u2 = ((uTemp * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//							DWORD v2 = ((vTemp * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
							DWORD u2 = (uTemp * m_pTexture->m_Width ) >> c_FixedPointBits;
							DWORD v2 = (vTemp * m_pTexture->m_Height) >> c_FixedPointBits;

							if (u2 >= m_pTexture->m_Width) {
								u2 = m_pTexture->m_Width - 1;
							}
							if (v2 >= m_pTexture->m_Height) {
								v2 = m_pTexture->m_Height - 1;
							}

							DWORD pixel = m_pTexture->GetPixel(u2, v2);

							if (pixel) {
								pDepth[x] = z;
								pLine[x]  = m_pLightMap[LumaIndex(l,x,y) | pixel];

								if (shapeID) {
									pShape[x] = shapeID;
								}
							}
						}

						z += zInc;
						u += uInc;
						v += vInc;
						l += lInc;
					}
#else
					for (long x = xl; x < xr; ++x) {
						long mul = FixedDiv(((x << c_FixedPointBits) - xL), (xR - xL));

						long z = FixedMul((zR - zL), mul) + zL;

						if (pDepth[x] < z) {
							long u = FixedMul((uR - uL), mul) + uL;
							long v = FixedMul((vR - vL), mul) + vL;

							u = FixedDiv(u, z);
							v = FixedDiv(v, z);

							long l = FixedMul((lR - lL), mul) + lL;

							DWORD pixel = SampleTexturePoint(u, v, l);

							if (pixel) {
								pDepth[x] = z;
								pLine[x]  = pixel;

								if (shapeID) {
									pShape[x] = shapeID;
								}
							}
						}
					}
#endif
				}
			}
		}

		if (splitLeft) {
			left.x0 = left.x1;
			left.x1 = left.xSplit;
			left.y0 = left.y1;
			left.y1 = left.ySplit;
			left.z0 = left.z1;
			left.z1 = left.zSplit;
			left.u0 = left.u1;
			left.u1 = left.uSplit;
			left.v0 = left.v1;
			left.v1 = left.vSplit;
			left.l0 = left.l1;
			left.l1 = left.lSplit;
		}
		else {
			right.x0 = right.x1;
			right.x1 = right.xSplit;
			right.y0 = right.y1;
			right.y1 = right.ySplit;
			right.z0 = right.z1;
			right.z1 = right.zSplit;
			right.u0 = right.u1;
			right.u1 = right.uSplit;
			right.v0 = right.v1;
			right.v1 = right.vSplit;
			right.l0 = right.l1;
			right.l1 = right.lSplit;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawTrianglePerspective()
//
//	This is a much faster version of DrawTriangle().  The difference is that
//	it does not divide texture coords by z, so the rendered triangle will
//	have perspective distortion.  This can be useful for rendering small
//	(e.g., distant) triangles.  The trade-off is that small triangles take
//	more effort to set-up than to render, so using this function for distant
//	triangles only provides a small improvement over DrawTriangle().
//
void CwaRaster::DrawTrianglePerspective(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID)
{
	if ((pA->y > pB->y) || (pA->y > pC->y)) {
		if (pB->y < pC->y) {
			Vertex_t *pT = pA;
			pA = pB;
			pB = pC;
			pC = pT;
		}
		else {
			Vertex_t *pT = pA;
			pA = pC;
			pC = pB;
			pB = pT;
		}
	}

	long yMid, yBottom;

	long yTop = (pA->y + c_FixedPointMask) >> c_FixedPointBits;

	ScanLine_t left, right;

	bool splitLeft;

	if (pB->y < pC->y) {
		yMid    = (pB->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pC->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = false;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.u0     = pA->u;
		left.u1     = pC->u;
		left.v0     = pA->v;
		left.v1     = pC->v;
		left.l0     = pA->l;
		left.l1     = pC->l;

		left.xSplit = 0;
		left.ySplit = 0;
		left.zSplit = 0;
		left.uSplit = 0;
		left.vSplit = 0;
		left.lSplit = 0;

		right.x0     = pA->x;
		right.x1     = pB->x;
		right.xSplit = pC->x;
		right.y0     = pA->y;
		right.y1     = pB->y;
		right.ySplit = pC->y;
		right.z0     = FixedDiv(c_FixedPointOne, pA->z);
		right.z1     = FixedDiv(c_FixedPointOne, pB->z);
		right.zSplit = FixedDiv(c_FixedPointOne, pC->z);
		right.u0     = pA->u;
		right.u1     = pB->u;
		right.uSplit = pC->u;
		right.v0     = pA->v;
		right.v1     = pB->v;
		right.vSplit = pC->v;
		right.l0     = pA->l;
		right.l1     = pB->l;
		right.lSplit = pC->l;
	}
	else {
		yMid    = (pC->y + c_FixedPointMask) >> c_FixedPointBits;
		yBottom = (pB->y + c_FixedPointMask) >> c_FixedPointBits;

		splitLeft = true;

		left.x0     = pA->x;
		left.x1     = pC->x;
		left.xSplit = pB->x;
		left.y0     = pA->y;
		left.y1     = pC->y;
		left.ySplit = pB->y;
		left.z0     = FixedDiv(c_FixedPointOne, pA->z);
		left.z1     = FixedDiv(c_FixedPointOne, pC->z);
		left.zSplit = FixedDiv(c_FixedPointOne, pB->z);
		left.u0     = pA->u;
		left.u1     = pC->u;
		left.uSplit = pB->u;
		left.v0     = pA->v;
		left.v1     = pC->v;
		left.vSplit = pB->v;
		left.l0     = pA->l;
		left.l1     = pC->l;
		left.lSplit = pB->l;

		right.x0    = pA->x;
		right.x1    = pB->x;
		right.y0    = pA->y;
		right.y1    = pB->y;
		right.z0    = FixedDiv(c_FixedPointOne, pA->z);
		right.z1    = FixedDiv(c_FixedPointOne, pB->z);
		right.u0    = pA->u;
		right.u1    = pB->u;
		right.v0    = pA->v;
		right.v1    = pB->v;
		right.l0    = pA->l;
		right.l1    = pB->l;

		right.xSplit = 0;
		right.ySplit = 0;
		right.zSplit = 0;
		right.uSplit = 0;
		right.vSplit = 0;
		right.lSplit = 0;
	}

	long  y      = yTop;
	long  yStop  = yMid;
	DWORD repeat = 2;
	
	do {
		for ( ; y < yStop; ++y) {

			if (DWORD(y) < m_ViewHeight) {
				long mulL = FixedDiv(((y << c_FixedPointBits) - left.y0),  (left.y1  - left.y0));
				long mulR = FixedDiv(((y << c_FixedPointBits) - right.y0), (right.y1 - right.y0));

				long xL = FixedMul((left.x1  - left.x0),  mulL) + left.x0;
				long xR = FixedMul((right.x1 - right.x0), mulR) + right.x0;

				long xl = (xL + c_FixedPointMask) >> c_FixedPointBits;
				long xr = (xR + c_FixedPointMask) >> c_FixedPointBits;

				if (xl < 0) {
					xl = 0;
				}

				if (xr > long(m_ViewWidth)) {
					xr = long(m_ViewWidth);
				}

				if (xl < xr) {
					long zL = FixedMul((left.z1  - left.z0),  mulL) + left.z0;
					long zR = FixedMul((right.z1 - right.z0), mulR) + right.z0;

					long uL = FixedMul((left.u1  - left.u0),  mulL) + left.u0;
					long uR = FixedMul((right.u1 - right.u0), mulR) + right.u0;

					long vL = FixedMul((left.v1  - left.v0),  mulL) + left.v0;
					long vR = FixedMul((right.v1 - right.v0), mulR) + right.v0;

					long lL = FixedMul((left.l1  - left.l0),  mulL) + left.l0;
					long lR = FixedMul((right.l1 - right.l0), mulR) + right.l0;

					BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
					DWORD *pShape = m_pShapeID + (y * m_FramePitch);
					long  *pDepth = m_pDepth   + (y * m_FramePitch);

#if 1
					long mul0 = FixedDiv(((xl << c_FixedPointBits) - xL), (xR - xL));
					long mul1 = FixedDiv(((xr << c_FixedPointBits) - xL), (xR - xL));

					long ender;

					long z    = (FixedMul((zR - zL), mul0) + zL) << c_ZDepthBias;
					ender     = (FixedMul((zR - zL), mul1) + zL) << c_ZDepthBias;
					long zInc = (ender - z) / (xr - xl);

					long u    = FixedMul((uR - uL), mul0) + uL;
					ender     = FixedMul((uR - uL), mul1) + uL;
					long uInc = (ender - u) / (xr - xl);

					long v    = FixedMul((vR - vL), mul0) + vL;
					ender     = FixedMul((vR - vL), mul1) + vL;
					long vInc = (ender - v) / (xr - xl);

					long l    = FixedMul((lR - lL), mul0) + lL;
					ender     = FixedMul((lR - lL), mul1) + lL;
					long lInc = (ender - l) / (xr - xl);

					for (long x = xl; x < xr; ++x) {
						if (pDepth[x] < z) {
							pDepth[x] = z;

//							DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//							DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
							DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
							DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

							if (u2 >= m_pTexture->m_Width) {
								u2 = m_pTexture->m_Width - 1;
							}
							if (v2 >= m_pTexture->m_Height) {
								v2 = m_pTexture->m_Height - 1;
							}

							DWORD pixel = m_pTexture->GetPixel(u2, v2);

							pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

							if (shapeID) {
								pShape[x] = shapeID;
							}
						}

						z += zInc;
						u += uInc;
						v += vInc;
						l += lInc;
					}
#else
					for (long x = xl; x < xr; ++x) {
						long mulX = FixedDiv(((x << c_FixedPointBits) - xL), (xR - xL));

						long z = (FixedMul((zR - zL), mulX) + zL) << c_ZDepthBias;

						if (pDepth[x] < z) {
							pDepth[x] = z;

							long u = FixedMul((uR - uL), mulX) + uL;
							long v = FixedMul((vR - vL), mulX) + vL;
							long l = FixedMul((lR - lL), mulX) + lL;

							DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
							DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;

							if (u2 >= m_pTexture->m_Width) {
								u2 = m_pTexture->m_Width - 1;
							}
							if (v2 >= m_pTexture->m_Height) {
								v2 = m_pTexture->m_Height - 1;
							}

							DWORD pixel = m_pTexture->GetPixel(u2, v2);

							pLine[x] = m_pLightMap[LumaIndex(l,x,y) | pixel];

							if (shapeID) {
								pShape[x] = shapeID;
							}
						}
					}
#endif
				}
			}
		}

		if (splitLeft) {
			left.x0 = left.x1;
			left.x1 = left.xSplit;
			left.y0 = left.y1;
			left.y1 = left.ySplit;
			left.z0 = left.z1;
			left.z1 = left.zSplit;
			left.u0 = left.u1;
			left.u1 = left.uSplit;
			left.v0 = left.v1;
			left.v1 = left.vSplit;
			left.l0 = left.l1;
			left.l1 = left.lSplit;
		}
		else {
			right.x0 = right.x1;
			right.x1 = right.xSplit;
			right.y0 = right.y1;
			right.y1 = right.ySplit;
			right.z0 = right.z1;
			right.z1 = right.zSplit;
			right.u0 = right.u1;
			right.u1 = right.uSplit;
			right.v0 = right.v1;
			right.v1 = right.vSplit;
			right.l0 = right.l1;
			right.l1 = right.lSplit;
		}

		yStop = yBottom;

	} while (--repeat > 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawSprite()
//
//	Draws a 2D sprite to the screen.  The sprite is rectangular,
//	perpendicular to the view direction, and not subjected to any form of
//	perspective distortion.  This allows for vastly simplified rendering
//	code.
//
//	Sprites are transparent, so pixels == 0 are not drawn.
//
void CwaRaster::DrawSprite(Vertex_t corners[], DWORD shapeID)
{
	// Since the sides are parallel to one another, we only need to compute
	// the frame coords for each side of the rectangle once.
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[2].y + c_FixedPointMask) >> c_FixedPointBits;

	// Clamp the coords to the bounds of the frame buffer.

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	// Compute 1/z.  This only needs to be done once, since every pixel
	// that will be drawn will have the same depth value.
	long z = FixedDiv(c_FixedPointOne, corners[0].z) << c_ZDepthBias;

	for (long y = top; y < bottom; ++y) {
		long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[2].y - corners[0].y));

		if (DWORD(y) < m_ViewHeight) {
			BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
			DWORD *pShape = m_pShapeID + (y * m_FramePitch);
			long  *pDepth = m_pDepth   + (y * m_FramePitch);

			for (long x = left; x < right; ++x) {
				if (pDepth[x] < z) {

					// FIXME: replace division with incrementing
					long u = FixedDiv(((x << c_FixedPointBits) - corners[0].x), (corners[1].x - corners[0].x));

//					DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//					DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
					DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
					DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

					if (u2 >= m_pTexture->m_Width) {
						u2 = m_pTexture->m_Width - 1;
					}
					if (v2 >= m_pTexture->m_Height) {
						v2 = m_pTexture->m_Height - 1;
					}

					BYTE pixel = m_pTexture->GetPixel(u2, v2);

					if (pixel) {
						pLine[x]  = m_pLightMap[LumaIndex(corners[0].l,x,y) | DWORD(pixel)];
						pShape[x] = shapeID;
						pDepth[x] = z;
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawSpriteMirror()
//
//	Same as DrawSprite(), except the image is flipped left-to-right.  This
//	is needed for reversing creature sprites when viewed from the "wrong"
//	side.
//
void CwaRaster::DrawSpriteMirror(Vertex_t corners[], DWORD shapeID)
{
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[2].y + c_FixedPointMask) >> c_FixedPointBits;

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	long z = FixedDiv(c_FixedPointOne, corners[0].z) << c_ZDepthBias;

	for (long y = top; y < bottom; ++y) {
		long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[2].y - corners[0].y));

		if (DWORD(y) < m_ViewHeight) {
			BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
			DWORD *pShape = m_pShapeID + (y * m_FramePitch);
			long  *pDepth = m_pDepth   + (y * m_FramePitch);

			for (long x = left; x < right; ++x) {
				if (pDepth[x] < z) {

					// FIXME: replace division with incrementing
					long u = c_FixedPointOne - FixedDiv(((x << c_FixedPointBits) - corners[0].x), (corners[1].x - corners[0].x));

//					DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//					DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
					DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
					DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

					if (u2 >= m_pTexture->m_Width) {
						u2 = m_pTexture->m_Width - 1;
					}
					if (v2 >= m_pTexture->m_Height) {
						v2 = m_pTexture->m_Height - 1;
					}

					BYTE pixel = m_pTexture->GetPixel(u2, v2);

					if (pixel) {
						pLine[x]  = m_pLightMap[LumaIndex(corners[0].l,x,y) | DWORD(pixel)];
						pShape[x] = shapeID;
						pDepth[x] = z;
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawSpriteTranslucent()
//
//	This is a modified version of DrawSprite().  This is used when drawing
//	ghosts and wraiths.  The "colors" of a ghost/wraith are not colors,
//	but instead are darkness values that are used to darken the colors in
//	the frame buffer.  This appears to be the only special case where this
//	trick is used.
//
void CwaRaster::DrawSpriteTranslucent(Vertex_t corners[], DWORD shapeID)
{
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[2].y + c_FixedPointMask) >> c_FixedPointBits;

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	long z = FixedDiv(c_FixedPointOne, corners[0].z) << c_ZDepthBias;

	for (long y = top; y < bottom; ++y) {
		long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[2].y - corners[0].y));

		if (DWORD(y) < m_ViewHeight) {
			BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
			DWORD *pShape = m_pShapeID + (y * m_FramePitch);
			long  *pDepth = m_pDepth   + (y * m_FramePitch);

			for (long x = left; x < right; ++x) {
				if (pDepth[x] < z) {

					// FIXME: replace division with incrementing
					long u = FixedDiv(((x << c_FixedPointBits) - corners[0].x), (corners[1].x - corners[0].x));

//					DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//					DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
					DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
					DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

					if (u2 >= m_pTexture->m_Width) {
						u2 = m_pTexture->m_Width - 1;
					}
					if (v2 >= m_pTexture->m_Height) {
						v2 = m_pTexture->m_Height - 1;
					}

					BYTE pixel = m_pTexture->GetPixel(u2, v2);

					if (pixel) {
						// Colors < 14 are really darkness values.  Read the
						// color value already in the frame buffer, darken it
						// by the amount stored in the texture, and write the
						// result back to the frame buffer.
						if (pixel < BYTE(14)) {
							pLine[x] = m_pLightMap[(DWORD(13-pixel) << 8) | DWORD(pLine[x])];
						}

						// All other values are treated as normal colors.
						// For ghosts/wraiths, these are the eyes.
						else {
							pLine[x] = pixel;
						}

						pShape[x] = shapeID;
						pDepth[x] = z;
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawSpriteTranslucentMirror()
//
void CwaRaster::DrawSpriteTranslucentMirror(Vertex_t corners[], DWORD shapeID)
{
	long left   = (corners[0].x + c_FixedPointMask) >> c_FixedPointBits;
	long right  = (corners[1].x + c_FixedPointMask) >> c_FixedPointBits;
	long top    = (corners[0].y + c_FixedPointMask) >> c_FixedPointBits;
	long bottom = (corners[2].y + c_FixedPointMask) >> c_FixedPointBits;

	if (left < 0) {
		left = 0;
	}

	if (right > long(m_ViewWidth)) {
		right = long(m_ViewWidth);
	}

	if (top < 0) {
		top = 0;
	}

	if (bottom > long(m_ViewHeight)) {
		bottom = long(m_ViewHeight);
	}

	if (right < left) {
		return;
	}

	long z = FixedDiv(c_FixedPointOne, corners[0].z) << c_ZDepthBias;

	for (long y = top; y < bottom; ++y) {
		long v = FixedDiv(((y << c_FixedPointBits) - corners[0].y), (corners[2].y - corners[0].y));

		if (DWORD(y) < m_ViewHeight) {
			BYTE  *pLine  = m_pFrame   + (y * m_FramePitch);
			DWORD *pShape = m_pShapeID + (y * m_FramePitch);
			long  *pDepth = m_pDepth   + (y * m_FramePitch);

			for (long x = left; x < right; ++x) {
				if (pDepth[x] < z) {

					// FIXME: replace division with incrementing
					long u = c_FixedPointOne - FixedDiv(((x << c_FixedPointBits) - corners[0].x), (corners[1].x - corners[0].x));

//					DWORD u2 = ((u * m_pTexture->m_Width ) + c_FixedPointHalf) >> c_FixedPointBits;
//					DWORD v2 = ((v * m_pTexture->m_Height) + c_FixedPointHalf) >> c_FixedPointBits;
					DWORD u2 = (u * m_pTexture->m_Width ) >> c_FixedPointBits;
					DWORD v2 = (v * m_pTexture->m_Height) >> c_FixedPointBits;

					if (u2 >= m_pTexture->m_Width) {
						u2 = m_pTexture->m_Width - 1;
					}
					if (v2 >= m_pTexture->m_Height) {
						v2 = m_pTexture->m_Height - 1;
					}

					BYTE pixel = m_pTexture->GetPixel(u2, v2);

					if (pixel) {
						if (pixel < BYTE(14)) {
							pLine[x] = m_pLightMap[(DWORD(13-pixel) << 8) | DWORD(pLine[x])];
						}
						else {
							pLine[x] = pixel;
						}

						pShape[x] = shapeID;
						pDepth[x] = z;
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawNumber()
//
//	Simple wrapper around DrawString() that handles the integer-to-string
//	conversion.
//
void CwaRaster::DrawNumber(FontElement_t font[], DWORD x, DWORD y, long value, DWORD color)
{
	char buffer[32];
	sprintf(buffer, "%d", value);
	DrawString(font, x, y, buffer, color);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawSignedNumber()
//
//	Alternate version of DrawNumber() that forces a "+" sign for positive
//	numbers.  This is used when displaying armor class and ability modifiers.
//
void CwaRaster::DrawSignedNumber(FontElement_t font[], DWORD x, DWORD y, long value, DWORD color)
{
	char buffer[32];
	sprintf(buffer, "%+d", value);
	DrawString(font, x, y, buffer, color);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DrawString()
//
//	Draws text to the frame buffer using the given font.  A font is stored
//	using a 16-bit mask for each line, along with with/height info.  For each
//	pixel on a line, only draw the color if the bit is set.
//
//	NOTE: No bounds checks are applied.  If the string extends outside the
//	bounds of the frame buffer, memory corruption will occur.
//
void CwaRaster::DrawString(FontElement_t font[], DWORD x, DWORD y, char message[], DWORD color)
{
	BYTE *pTarget = m_pFrame + (y * m_FramePitch) + x;

	DWORD length = DWORD(strlen(message));

	// Loop over each character in the string.
	for (DWORD cn = 0; cn < length; ++cn) {
		char sym = message[cn];

		// Only chars 32-127 exist in each font.  Turn anything else into a
		// space to avoid reference problems.
		if ((sym < 32) || (sym > 127)) {
			sym = 32;
		}

		FontElement_t &element = font[sym - 32];

		BYTE *pTC = pTarget;

		// Need three versions of the code: 320x200, 640x400, and 960x600.
		// This allows each pixel to be doubled or tripled as needed.

		if (m_FrameHeight < 400) {
			for (DWORD cy = 0; cy < element.Height; ++cy) {
				WORD mask = 0x8000;
				WORD bits = element.Lines[cy];

				for (DWORD cx = 0; cx < element.Width; ++cx) {
					if (0 != (bits & mask)) {
						pTC[cx] = BYTE(color);
					}

					mask = mask >> 1;
				}

				pTC += m_FramePitch;
			}

			pTarget += element.Width;
		}
		else if (m_FrameHeight < 600) {
			for (DWORD cy = 0; cy < element.Height*2; ++cy) {
				WORD mask = 0x8000;
				WORD bits = element.Lines[cy>>1];

				for (DWORD cx = 0; cx < element.Width; ++cx) {
					if (0 != (bits & mask)) {
						pTC[(cx<<1)  ] = BYTE(color);
						pTC[(cx<<1)+1] = BYTE(color);
					}

					mask = mask >> 1;
				}

				pTC += m_FramePitch;
			}

			pTarget += element.Width * 2;
		}
		else {
			for (DWORD cy = 0; cy < element.Height*3; ++cy) {
				WORD mask = 0x8000;
				WORD bits = element.Lines[cy/3];

				for (DWORD cx = 0; cx < element.Width; ++cx) {
					if (0 != (bits & mask)) {
						pTC[(cx*3)  ] = BYTE(color);
						pTC[(cx*3)+1] = BYTE(color);
						pTC[(cx*3)+2] = BYTE(color);
					}

					mask = mask >> 1;
				}

				pTC += m_FramePitch;
			}

			pTarget += element.Width * 3;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	StringLength()
//
//	Given a string, compute the number of pixels required to display that
//	string.
//
//	This does not scale the size value for the current frame buffer resolution.
//	If the size needs to be doubled or tripled, the caller needs to do it.
//
//	Carriage returns are not supported.  Only give a single line of text.
//
DWORD CwaRaster::StringLength(FontElement_t font[], char message[])
{
	DWORD length = 0;

	char *pSym = message;

	while ('\0' != *pSym) {
		char sym = *(pSym++);

		if ((sym < 32) || (sym > 127)) {
			sym = 32;
		}

		length += font[sym - 32].Width;
	}

	return length;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetLightMap()
//
//	Assigns a new lightmap to the rasterizer.  This is used to fade colors
//	towards black or fog.
//
void CwaRaster::SetLightMap(BYTE *pLightMap)
{
	m_pLightMap = pLightMap;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetPalette()
//
//	Assigns a default color palette to the frame buffer.  This should be
//	called at the start of each frame.  The blitting code will test to see
//	if a given texture has a custom palette.  If so, that palette will
//	replace the default one, since it assumes it has been given a full-frame
//	image with a custom palette (which is normal for all full-frame images
//	in Arena, such as character screen background, main-dungeon splash
//	pages, the automap and journal, etc.).
//
void CwaRaster::SetPalette(DWORD *pPalette)
{
	m_pPalette = pPalette;
}


/////////////////////////////////////////////////////////////////////////////
//
//	SetTexture()
//
//	Assigns a new texture for rendering.  If given a NULL pointer, this will
//	reset rasterization back to use the default (checkerboard) pattern.
//
void CwaRaster::SetTexture(CwaTexture *pTexture)
{
	if (NULL == pTexture) {
		m_pTexture = &m_DefaultTexture;
	}
	else {
		m_pTexture = pTexture;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	CountNonzeroPixels()
//
//	Number of pixels actually rendered into the frame buffer.  Used for
//	early rasterization tests.  Obsolete, and probably could be removed now.
//
DWORD CwaRaster::CountNonzeroPixels(void)
{
	DWORD count = 0;

	for (DWORD i = 0; i < m_PixelCount; ++i) {
		if (0 != m_pFrame[i]) {
			++count;
		}
	}

	return count;
}


/////////////////////////////////////////////////////////////////////////////
//
//	FixedSqrt()
//
//	Iterative square root function for 17.15 fixed-point numbers.  Since this
//	effectively cuts the number of bits in half, numerical adjustment is done
//	to make the result an 8.8 value, which is shifted up 7 bits to produce an
//	8.15 value, with the lower 7 bits zeroed out.
//
//	However, for this app, square roots are used only in world space, where
//	units are in terms of cells (or blocks, however you prefer to think of
//	them).  So more fractional precision is preferable.  To deal with this,
//	the code shifts the incoming value upwards to make a 12.20 formatted
//	value, finds the root, which produces a 6.10 value.  So we need to shift
//	the result up to turn it back into a 17.15 value.
//
//	In spite of the bit shifting, the final result is therefore a 6.10 value,
//	so the resulting value cannot be larger than 0x3F.FFC, or 63.999.
//
//	This means that values greater than 2^12 will wrap around.  Square roots
//	should be positive, and the values are treated as unsigned, so the sign
//	bit is thrown away during the bit shift.
//
long FixedSqrt(DWORD a)
{
	PROFILE(PFID_FixedSqrt);

	// Shift the incoming value upwards 5 bits so the value being processed
	// effectively has 20 bits of fraction, even though the low 5 bits are
	// all zeroes.
	a <<= 5;

	DWORD rem     = 0;
	DWORD root    = 0;
	DWORD divisor = 0;

	for(long i=0; i < 16; i++) {
		root <<= 1;
		rem = ((rem << 2) + (a >> 30));
		a <<= 2;
		divisor = (root<<1) + 1;
		if(divisor <= rem) {
			rem -= divisor;
			root++;
		}
	}

	// The result is a 6.10 value.  Need to shift the result up to turn it
	// back into a 6.15 value.

	return long(root << 5);
}


long g_FixedDistanceLUT[3 << c_DistanceLutBits][3 << c_DistanceLutBits];


/////////////////////////////////////////////////////////////////////////////
//
//	FixedDistance()
//
long FixedDistance(long x, long z)
{
	PROFILE(PFID_FixedDistance);

	if (x < 0) {
		x = -x;
	}
	if (z < 0) {
		z = -z;
	}

	if ((x < IntToFixed(3)) && (z < IntToFixed(3))) {
		return FixedDistanceMacro(x, z);
	}

	return FixedSqrt(FixedMul(x, x) + FixedMul(z, z));
}


/////////////////////////////////////////////////////////////////////////////
//
//	InitFixedDistanceLUT()
//
void InitFixedDistanceLUT(void)
{
	for (long z = 0; z < (3 << c_DistanceLutBits); ++z) {
		long z2 = z << (c_FixedPointBits - c_DistanceLutBits);
		for (long x = 0; x < (3 << c_DistanceLutBits); ++x) {
			long x2 = x << (c_FixedPointBits - c_DistanceLutBits);
			g_FixedDistanceLUT[z][x] = FixedSqrt(FixedMul(x2, x2) + FixedMul(z2, z2));
		}
	}
}


