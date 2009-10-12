/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/ParseBSA.h 10    12/31/06 10:51a Lee $
//
//	File: ParseBSA.h
//
//
//	Basic layout of Globals.BSA:
//	 * 16-bit word indicating total number of index entries at end of file
//	 * all data files packed one after another
//	 * array of index values
//
//	To scan through Globals.BSA, first read the 16-bit from the start of the
//	file.  This indicates how many files are packed into Globals.BSA, which
//	is used to scan backwards from the end of the file to locate the start
//	of the index.  Then scan through the index from first to last, adding up
//	the size of each file (starting with an initial size of 2 bytes for the
//	file/index count value).
//
//	Arena.exe appears to first scan through its directory to see if a named
//	file can be loaded from there.  If not, it pulls the data from the BSA
//	file.  This allows the patches to replace a few of the INF files (which
//	are also special, in that the INF files stored in Globals.BSA are encoded
//	using a bit hashing technique, but the ones in the Arena directory are
//	plain text).
//
//	WARNING: The list of files locating the BSA index are *ALMOST* sorted,
//	but not completely (e.g., EVLINTRO.XMI and EVLINTRO.XFM are reversed).
//	Doing a binary look-up of a file works 99% of the time, but will sometimes
//	fail.  My guess is that the look-up code in Arena.exe scans through the
//	index in order, adding up the size values to compute the file's offset
//	within Globals.BSA so it doesn't need to cache the offsets.  The code
//	implemented here will cache those values, and does do binary look-up,
//	but falls back to a linear search if it could not locate the file.
//
//
//	All of the INF files use a bit-hashing technique to encode them into
//	something unreadable (unless you like reading hex dumps and looking for
//	repeating patterns -- thankfully the patched version of the game includes
//	plain-text versions of some of the INF files that are almost identical to
//	the ones stored in Globals.BSA, at least for the first several hundred
//	bytes).
//
//	To decode INF files, MakeInverseCodeTable() needs to be called once.
//	Currently this is done from within BSALoadResources(), so it is already
//	taken care of once Global.BSA is available for processing.  This will
//	build some look-up tables that are used to decode the hashed bits.  Note
//	that the INF files only contain 7-bit ASCII.  There are no symbols above
//	127, and the fonts in the game only define values for symbols 32 - 127.
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


#pragma pack(push, normalPacking)
#pragma pack(1)


//
// This is the format of the index entries that occur at the end of the
// Globals.BSA file.
//
struct BSAIndex_t
{
	char Name[12];	// 8.3 formatted file name, without any '\0' terminator
	WORD U1;		// Unkown, possibly unused
	WORD Size;
	WORD U2;		// Unkown, possibly unused
};


//
// This is the struct used to cache index info into the BSA file.  An initial
// pass over the raw index is done to compute the offsets, and to fix up the
// file name so it's useable by string operations.
//
// ParseGlobalIndex() is responsible for doing all of this processing.
//
struct GlobalIndex_t
{
	char  Name[16];	// fixed version of file name with '\0', padded to DWORD alignment
	DWORD Offset;	// byte offset from start of BSA file
	DWORD Size;		// size of resource, in bytes
};


//
// This is the header that is found at the beginning of CFA files, which are
// used for creature animations.  Refer to LoadCFA() for the specifics on how
// animations are stored, and the work required to decode them.
//
struct HeaderCFA_t
{
	WORD WidthUncompressed;		// actual width of the image
	WORD Height;
	WORD WidthCompressed;		// bytes/line of bit-packed image
	WORD u2;
	WORD u3;
	BYTE BitsPerPixel;			// indicates how to demux the encoded pixels
	BYTE FrameCount;
	WORD HeaderSize;			// this also includes the color LUT following the header
};


struct HeaderCIF_t
{
	WORD u1;
	WORD u2;
	WORD Width;
	WORD Height;
	WORD Flags;
	WORD RecordSize;
};


//
// DFA files are used to static animations, such as fire bits, bartenders,
// jugglers, and other decorative sprites.  These are typically a static
// frame with a small block of animation.
//
struct HeaderDFA_t
{
	WORD FrameCount;
	WORD u2;
	WORD u3;
	WORD Width;
	WORD Height;
	WORD CompressedSize;
};


#define IMG_FLAG_PIXELCOUNT	0x0008			// indicates uncompressed size is stored at offset 0x0C
#define IMG_FLAG_PALETTE	0x0100			// indicates last 0x300 bytes contain a custom palette
#define IMG_FLAG_MYSTERY1	0x0004
#define IMG_FLAG_MYSTERY2	0x0800

// It's not clear what the two mystery flags indicate.  They are usually
// present with compressed images, but not always.  And are set for some
// of the non-compressed images.  These may be hints for how to interpret
// the palette.

struct HeaderIMG_t
{
	WORD OffsetX;
	WORD OffsetY;
	WORD Width;
	WORD Height;
	WORD Flags;				// unknown, 0x8000 == compressed? or 0x00800 maybe?
	WORD RecordSize;		// size of record - 12 for most images, add 12 to get offset to palette for splash screens
	WORD UncompressedSize;	// size of image when uncompressed, only present if IMG_FLAG_PIXELCOUNT flag is set
};


//
// Header block found at the beginning of each sound file.  These are Creative
// Voice files, but there does not appear to be any information about the
// audio formatting.  All audio is 8-bit mono.  I have no idea whether these
// are linear, A-law, or mu-law encoded.  The format bits that look like they
// should indicate this are not set, so I assume they're linear.  Mixing them
// by linear addition doesn't seem to change the value, so I assume linear,
// but I'm an expert on video, not audio.
//
struct HeaderVOC_t
{
	char  CreativeVoice[19];
	BYTE  MarkEOF;
	WORD  DataOffset;
	WORD  Version;				// 0x010A
	WORD  TwosCompVersion;		// 0x1129
};


//
// Fonts are stored in a packed bit format, one bit per pixel, with each line
// of a symbol packed into 8 or 16 bits, with an array of lines-per-symbol
// values stored at the beginning of the font file.
//
// Once a font is decoded, it is packed into an array of FontElement_t structs
// so they can be easily accessed to mask chars into place by rendering code.
//
struct FontElement_t
{
	DWORD Width;
	DWORD Height;
	WORD  Lines[16];
};


#pragma pack(pop, normalPacking)


extern char g_ArenaPath[];				// fill with Arena's path, ".\" or "C:\Arena\"


void MakeArenaDirectory(char name[]);


enum ArenaPalette_t
{
	Palette_Default		= 0,
	Palette_CharSheet	= 1,
	Palette_Daytime		= 2,
	Palette_Dreary		= 3,
	Palette_ArraySize	= 4
};


enum ArenaFont_t
{
	Font_A,				// height = 11
	Font_B,				// height =  6
	Font_C,				// height = 14
	Font_D,				// height =  7
	Font_S,				// height =  5
	Font_4,				// height =  7
	Font_Arena,			// height =  9	--  used for stat values in character sheet
	Font_Char,			// height =  8  --  used for stat field names in character sheet
	Font_Teeny,			// height =  8
	Font_ArraySize
};


class ArenaLoader
{
private:
	struct PrivateIndex_t
	{
		char  Name[16];
		DWORD Size;
		DWORD Offset;
		bool  HasOverride;
	};

	PrivateIndex_t*	m_pIndex;

	DWORD			m_Palette[Palette_ArraySize][256];

	FontElement_t	m_FontList[Font_ArraySize][96];

	DWORD			m_IndexCount;
	GlobalIndex_t*	m_pGlobalIndex;

	DWORD			m_GlobalDataSize;
	BYTE*			m_pGlobalData;

	// A large scratch buffer is used to hold temporary data, usually while
	// decompressing images.
	//
	BYTE*			m_pScratch;

	// For normal resource access, there will be a pointer directly into the
	// data withing m_pGlobalData.  However, some resources are located
	// directly in the Arena directory.  Those will be loaded into this
	// buffer for processing.
	//
	BYTE*			m_pFileScratch;

public:
	ArenaLoader(void);
	~ArenaLoader(void);

	bool   LoadResources(void);
	void   FreeResources(void);

	void   ParseIndex(BYTE *pData, GlobalIndex_t *pIndex, DWORD entryCount);

	BYTE*  LoadFile(char name[], DWORD &size, bool *pFromBSA = NULL);

	DWORD  LookUpResource(char name[]);

	void   LoadPalette(char filename[], DWORD palette[]);
	DWORD* GetPaletteARGB(ArenaPalette_t select);
	void   GetPaletteARGB(ArenaPalette_t select, DWORD palette[]);
	void   GetPaletteBlueFirst(ArenaPalette_t select, BYTE palette[]);
	void   GetPaletteRedFirst(ArenaPalette_t select, BYTE palette[]);

	void           LoadFont(char filename[], DWORD fontHeight, FontElement_t symbols[]);
	FontElement_t* GetFont(ArenaFont_t font);

	void   LoadLightMap(char name[], BYTE buffer[], bool dump = false);

	BYTE*  LoadData(char name[], DWORD &dataSize);

	BYTE*  LoadSound(char name[], DWORD &sampleCount, DWORD &sampleRate);
	void   DumpSounds(void);

	void   CheckCFA(DWORD id, DWORD &width, DWORD &height, DWORD &frameCount);
	void   LoadCFA(DWORD id, DWORD width, DWORD height, DWORD pitch, DWORD frameCount, BYTE *ppFrames[]);
	void   DumpCFA(void);

	void   CheckDFA(DWORD id, DWORD &width, DWORD &height, DWORD &frameCount);
	void   LoadDFA(DWORD id, DWORD width, DWORD height, DWORD pitch, DWORD frameNum, BYTE *pResult);
	void   DumpDFA(void);

	void   CheckCIF(char name[], DWORD frameNum, DWORD &width, DWORD &height, DWORD &offsetX, DWORD &offsetY);
	void   LoadCIF(char name[], DWORD width, DWORD height, DWORD frameNum, BYTE *pResult);
	void   DumpCIF(void);

	void   CheckIMG(char name[], DWORD &width, DWORD &height, DWORD &offsetX, DWORD &offsetY, bool &hasHeader);
	DWORD* LoadIMG(char name[], DWORD width, DWORD height, DWORD pitch, bool hasHeader, BYTE *pResult);
	void   DumpIMG(char filename[]);
	void   DumpIMGRAW(char filename[], DWORD width, DWORD height, DWORD palette32[]);
	void   DumpIMGFAT(char filename[]);
	void   DumpImages(void);
	void   ScanIMGs(void);

	void   LoadWallTexture(DWORD index, BYTE *pPixels, DWORD offset);
	void   LoadWater(char name[], DWORD frameNum, BYTE *pPixels);

	void   MakeInverseCodeTable(void);
	void   DeobfuscateINF(BYTE text[], DWORD count);

	void   DumpBSAIndex(void);
	void   DumpInfo(void);
	void   DumpMap(char resname[], char prefix[]);
	void   DumpMaps(void);
	void   DumpWalls(void);

	void   ExtractFile(char filename[]);
	void   HexDump(char filename[], char path[] = "", bool rightToLeft = false);
};



extern ArenaLoader* g_pArenaLoader;


