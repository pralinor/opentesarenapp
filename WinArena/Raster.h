/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Raster.h 8     12/31/06 10:55a Lee $
//
//	File: Raster.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#include "ParseBSA.h"
#include "Animation.h"


#define FloatToFixed(x)		(long((x) * float(c_FixedPointOne)))
#define IntToFixed(x)		((x) << c_FixedPointBits)
#define FixedFloor(x)		((x) & c_FixedPointWhole)
#define FixedCeil(x)		(((x) + c_FixedPointMask) & c_FixedPointWhole)


#define c_FixedPointBits	15
#define c_FixedPointMask	0x00007FFF
#define c_FixedPointWhole	0xFFFF8000
#define c_FixedPointOne		0x00008000
#define c_FixedPointHalf	0x00004000

// Extra bits used to store inverse Z value to increase precision.
#define c_ZDepthBias		4

#define c_ZNear				(c_FixedPointOne >> 2)
#define c_ZFar				(32 << c_FixedPointBits)
#define c_ZRange			(zFar - zNear)

#define c_FloorHeight		c_FixedPointOne
#define c_WallHeight		(IntToFixed(3) >> 2)
#define c_WallLength		(IntToFixed(5) >> 2)
#define c_EyeLevel			((c_WallHeight * 35) >> 6)		// how far above floor level to place camera
#define c_WaterLevel		(c_WallHeight / 5)				// how far below floor level to place camera when in water/trench
#define c_BulletLevel		((c_WallHeight * 30) >> 6)

#define c_LumaScale			13
#define c_MaxLuma			IntToFixed(c_LumaScale)

#define c_ZScale			(IntToFixed(6) >> 2)


/////////////////////////////////////////////////////////////////////////////
//
struct Vertex_t
{
	long x;
	long y;
	long z;
	long u;
	long v;
	long l;
};


class CwaRaster
{
private:
	BYTE*	m_pPrivateFrame;		// internal frame buffer, allocated and needs to be freed.
	BYTE*	m_pFrame;				// frame buffer provided by higher level, not managed by this object
	DWORD*	m_pPalette;				// color palette to use when displaying current frame
	BYTE*	m_pLightMap;			// current lightmap, used to fade pixel colors towards black or fog color
	DWORD*	m_pShapeID;				// ID buffer, stores ID value that maps back to the object rendered there
	long*	m_pDepth;				// depth buffer, storing 1/z
	DWORD	m_ViewWidth;			// size of rendering area; this exclude the HUD at the bottom of the screen
	DWORD	m_ViewHeight;
	DWORD	m_FrameHeight;			// size of frame buffer
	DWORD	m_FrameWidth;
	DWORD	m_FramePitch;
	DWORD	m_WindowWidth;			// resolution of the window where frame will be blitted
	DWORD	m_WindowHeight;
	DWORD	m_PixelCount;
	DWORD	m_BlitMode;

	CwaTexture	m_DefaultTexture;
	CwaTexture*	m_pTexture;

public:
	CwaRaster(void);
	~CwaRaster(void);

	void Allocate(DWORD width, DWORD height);
	void SetWindowSize(DWORD windowWidth, DWORD windowHeight);
	void SetBlitMode(DWORD mode);
	void SetFrameBuffer(BYTE *pBuffer, DWORD pitch);
	void ClearFrameBuffer(DWORD color, DWORD lineCount);
	void ClearDepthBuffer(void);
	void ScreenShot(char filename[]);

	void DisplayFrame(HDC hDC);

	void DrawBox(long x, long y, long width, long height, BYTE color);

	void Line(long x0, long y0, long x1, long y1, BYTE color);
	void Circle(long x0, long y0, long radius, BYTE color);
	void FilledCircle(long x0, long y0, long radius, BYTE color);
	void Polygon(long pX[], long pY[], DWORD count, BYTE color);

	void FillRect(long x, long y, long width, long height, long scale, BYTE color);

	void Blit(const CwaTexture &texture, DWORD posX, DWORD posY);
	void BlitRect(const CwaTexture &texture, DWORD posX, DWORD posY, DWORD dx, DWORD dy, DWORD width, DWORD height);
	void BlitTransparent(const CwaTexture &texture, DWORD posX, DWORD posY);
	void BlitTransparentMirror(const CwaTexture &texture, DWORD posX, DWORD posY);

	void PaintVerticalQuad(Vertex_t corners[]);
	void PaintVerticalQuadTransparent(Vertex_t corners[]);
	void PaintVerticalQuadDepthID(Vertex_t corners[], DWORD shapeID);

	void PaintNgon(Vertex_t corners[], DWORD cornerCount);
	void PaintNgonFast(Vertex_t corners[], DWORD cornerCount);
	void PaintSprite(Vertex_t corners[]);
	void PaintSpriteTranslucent(Vertex_t corners[]);

	void DrawTriangleSolid(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, BYTE color);
	void DrawTriangleTest(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC);
	void DrawTriangleTestPer(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC);
	void DrawTriangle(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID);
	void DrawTriangleTransparent(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID);
	void DrawTrianglePerspective(Vertex_t *pA, Vertex_t *pB, Vertex_t *pC, DWORD shapeID);

	void DrawSprite(Vertex_t corners[], DWORD shapeID);
	void DrawSpriteMirror(Vertex_t corners[], DWORD shapeID);
	void DrawSpriteTranslucent(Vertex_t corners[], DWORD shapeID);
	void DrawSpriteTranslucentMirror(Vertex_t corners[], DWORD shapeID);

	void  DrawNumber(FontElement_t font[], DWORD x, DWORD y, long value, DWORD color);
	void  DrawSignedNumber(FontElement_t font[], DWORD x, DWORD y, long value, DWORD color);
	void  DrawString(FontElement_t font[], DWORD x, DWORD y, char message[], DWORD color);
	DWORD StringLength(FontElement_t font[], char message[]);

	void  SetLightMap(BYTE *pLightMap);
	void  SetPalette(DWORD *pPalette);
	void  SetTexture(CwaTexture *pTexture);

	DWORD GetShapeID(DWORD x, DWORD y)		{ return m_pShapeID[(y * m_FramePitch) + x]; }
	long  GetPixelDepth(DWORD x, DWORD y)	{ return m_pDepth[(y * m_FramePitch) + x]; }
	BYTE* GetFrameBuffer(void)				{ return m_pFrame; }
	DWORD*GetPalette(void)					{ return m_pPalette; }

	DWORD CountNonzeroPixels(void);
	DWORD PixelCount(void)					{ return m_PixelCount; }

	DWORD FrameWidth(void)		{ return m_FrameWidth; }
	DWORD FrameHeight(void)		{ return m_FrameHeight; }
};


#define c_DistanceLutBits 6
extern long g_FixedDistanceLUT[3 << c_DistanceLutBits][3 << c_DistanceLutBits];


long FixedSqrt(DWORD a);
long FixedDistance(long x, long z);
void InitFixedDistanceLUT(void);

#define FixedDistanceMacro(x, z)	g_FixedDistanceLUT[z >> (c_FixedPointBits - c_DistanceLutBits)][x >> (c_FixedPointBits - c_DistanceLutBits)]

_forceinline
//inline
long FixedMul(long a, long b)
{
	__asm {
		mov  eax,a
		imul b
		shrd eax,edx,c_FixedPointBits
	}

	// eax is implicitly returned
}

_forceinline
//inline
long FixedDiv(long a, long b)
{
//	if (0 != b) {
		if ((a^b)&0x8000000) {
			__asm {
				mov  eax,a
				mov  ebx,b
				neg  eax
				xor  edx,edx
				shld edx,eax,c_FixedPointBits
				sal  eax,c_FixedPointBits
				idiv ebx
				neg  eax
			}
		}
		else {
			__asm {
				mov  eax,a
				mov  ebx,b
				xor  edx,edx
				shld edx,eax,c_FixedPointBits
				sal  eax,c_FixedPointBits
				idiv ebx
			}
		}
//	}
//	else {
//		__asm {
//			mov eax,0x7FFFFFFF
//		}
//	}

	// eax is implicitly returned
}

//#define FixedMul(r,a,b)   __asm mov eax,a   __asm mul b   __asm shrd eax,edx,16  __asm mul r,eax
/*
inline long SpriteVertexDepth(const Vertex_t &vert)
{
	return FixedMul(vert.z, vert.z);
//	return FixedMul(vert.x, vert.x)
//		 + FixedMul(vert.y, vert.y)
//		 + FixedMul(vert.z, vert.z);
}
*/
inline long VertexDepth(const Vertex_t &vert)
{
//	return vert.z;
	return FixedMul(vert.x, vert.x)
		 + FixedMul(vert.y, vert.y)
		 + FixedMul(vert.z, vert.z);
}


