/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/ParseBSA.cpp 10    12/31/06 10:51a Lee $
//
//	File: ParseBSA.cpp
//
//
/////////////////////////////////////////////////////////////////////////////


// I want my assertions, dagnabbit.
#undef NDEBUG


#include "WinArena.h"
#include <direct.h>
#include <assert.h>
#include "ParseBSA.h"
#include "TGA.h"
#include "GIFFile.h"


// This path needs to be initialized to the location of the Arena directory.
// This is probably "C:\Arena\", and should end with a "\".  If nothing else,
// set this to ".\" since it will always be prepended before all file names
// when directly accessing data on the disk.
//
char  g_ArenaPath[MAX_PATH];


// A global pointer is used to reference the one instance of the loader
// object.  This allows various parts of the program to access data files
// without having to pass around pointers to the loader.
//
ArenaLoader* g_pArenaLoader = NULL;


/////////////////////////////////////////////////////////////////////////////
//
//	MakeArenaDirectory()
//
//	Creates a new directory under the main Arena directory.  This is used to
//	guarantee a folder exists when exporting data, otherwise fopen() will
//	fail if the subfolder does not exist.
//
void MakeArenaDirectory(char name[])
{
	char dirname[256];
	sprintf(dirname, "%s%s", g_ArenaPath, name);
	_mkdir(dirname);
}



/////////////////////////////////////////////////////////////////////////////
//
//	constructor
//
ArenaLoader::ArenaLoader(void)
	:	m_pIndex(NULL),
		m_IndexCount(0),
		m_pGlobalIndex(NULL),
		m_GlobalDataSize(0),
		m_pGlobalData(NULL),
		m_pScratch(NULL),
		m_pFileScratch(NULL)
{
}


/////////////////////////////////////////////////////////////////////////////
//
//	destructor
//
ArenaLoader::~ArenaLoader(void)
{
	SafeDeleteArray(m_pFileScratch);
	SafeDeleteArray(m_pScratch);
	SafeDeleteArray(m_pGlobalData);
	SafeDeleteArray(m_pIndex);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadResources()
//
//	Loads all of the color palettes and fonts, and scans through the index in
//	Global.BSA.
//
//	Note that this code loads the entire Global.BSA file into a buffer and
//	keeps it there until FreeResources() is called, or the loader object is
//	destructed.  That file is a meager 16 megabytes, and any system that can't
//	spare that amount of memory probably won't have the power to run this app
//	with software rendering.
//
bool ArenaLoader::LoadResources(void)
{
	PROFILE(PFID_BSALoadResources);

	SafeDeleteArray(m_pScratch);
	SafeDeleteArray(m_pFileScratch);

	m_pScratch     = new BYTE[1024*1024];
	m_pFileScratch = new BYTE[512 * 1024];

	char filename[MAX_PATH];

	// Load the color maps.  Note that we keep a copy of the raw palette used
	// for most rendering.  This will be used when dumping some image data
	// to palettized TGA files for viewing.
	LoadPalette("Pal.col",     m_Palette[Palette_Default]);
	LoadPalette("Charsht.col", m_Palette[Palette_CharSheet]);
	LoadPalette("Daytime.col", m_Palette[Palette_Daytime]);
	LoadPalette("Dreary.col",  m_Palette[Palette_Dreary]);

	LoadFont("FONT_A.DAT",   11, m_FontList[Font_A]);
	LoadFont("FONT_B.DAT",    6, m_FontList[Font_B]);
	LoadFont("FONT_C.DAT",   14, m_FontList[Font_C]);
	LoadFont("FONT_D.DAT",    7, m_FontList[Font_D]);
	LoadFont("FONT_S.DAT",    5, m_FontList[Font_S]);
	LoadFont("FONT4.DAT",     7, m_FontList[Font_4]);
	LoadFont("ARENAFNT.DAT",  9, m_FontList[Font_Arena]);
	LoadFont("CHARFNT.DAT",   8, m_FontList[Font_Char]);
	LoadFont("TEENYFNT.DAT",  8, m_FontList[Font_Teeny]);

	// Read all of the BSA file into a buffer.
	sprintf(filename, "%sGlobal.BSA", g_ArenaPath);
	FILE *pFile = fopen(filename, "rb");
	if (NULL != pFile) {
		// Figure out how large the file is.
		fseek(pFile, 0, SEEK_END);
		m_GlobalDataSize = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);

		// Then we 
		m_pGlobalData = new BYTE[m_GlobalDataSize];

		if (NULL != m_pGlobalData) {
			fread(m_pGlobalData, 1, m_GlobalDataSize, pFile);
		}

		fclose(pFile);

		if (NULL == m_pGlobalData) {
			return false;
		}
	}
	else {
		return false;
	}

	// The first 16-bit word in the file contains a count of the number of
	// files/indices in the BSA.  This tells us how big of a global index
	// to allocate.  It also helps us locate the start of the index.  Given
	// the size of the index, we can scan backwards from the end of the
	// BSA file to locate the first index entry.

	m_IndexCount = reinterpret_cast<WORD*>(m_pGlobalData)[0];

	DWORD indexSize    = m_IndexCount * sizeof(BSAIndex_t);

	DWORD indexOffset  = m_GlobalDataSize - indexSize;

	m_pGlobalIndex     = new GlobalIndex_t[m_IndexCount];

	ParseIndex(m_pGlobalData + indexOffset, m_pGlobalIndex, m_IndexCount);

	MakeInverseCodeTable();

	return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	FreeResources()
//
void ArenaLoader::FreeResources(void)
{
	SafeDeleteArray(m_pGlobalIndex);
	SafeDeleteArray(m_pGlobalData);
	SafeDeleteArray(m_pScratch);
	SafeDeleteArray(m_pFileScratch);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadFile()
//
//	Utility function for accessing file data.  Arena first looks to see if a
//	file exists in raw format in its directory.  If not, then it will look
//	in Global.BSA for the data.
//
//	If a separate file exists, it will be read into memory in m_pFileScratch,
//	and a pointer to that buffer returned.  If the data resides in Global.BSA,
//	it will return a pointer to the start of the data located in m_pGlobalData.
//
//	The <pFromBSA> parameter will be set to true if the file was loaded from
//	Globals.BSA, or false if a copy of the file was 
//
BYTE* ArenaLoader::LoadFile(char name[], DWORD &size, bool *pFromBSA)
{
	char filename[256];
	sprintf(filename, "%s%s", g_ArenaPath, name);

	// Check for patch version of the file in the Arena directory.
	FILE *pFile = fopen(filename, "rb");
	if (NULL != pFile) {
		fseek(pFile, 0, SEEK_END);
		size = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);

		fread(m_pFileScratch, 1, size, pFile);

		fclose(pFile);

		if (NULL != pFromBSA) {
			*pFromBSA = false;
		}

		return m_pFileScratch;
	}

	// Otherwise the file should be packed into Global.BSA.

	DWORD id = LookUpResource(name);

	size = m_pGlobalIndex[id].Size;

	if (NULL != pFromBSA) {
		*pFromBSA = true;
	}

	return (m_pGlobalData + m_pGlobalIndex[id].Offset);
}


/////////////////////////////////////////////////////////////////////////////
//
//	ParseIndex()
//
//	Scans through the file index at the end of Global.BSA.  This will build
//	up a table of size and position data, indicating where each file is
//	located in Global.BSA.
//
void ArenaLoader::ParseIndex(BYTE *pData, GlobalIndex_t *pIndex, DWORD entryCount)
{
	BSAIndex_t *pFileIndex = reinterpret_cast<BSAIndex_t*>(pData);

	// Accumulate the starting offset of each file/chunk in the BSA.  Start
	// off with this set to 2, since we need to skip the 2-byte word at the
	// beginning of the file that contains the size of the index table.
	DWORD offset = 2;

	// Scan through the indices in order.  This is also the order in which
	// they are stored in the BSA file, which allows us to determine the
	// offset of the corresponding file/chunk in the BSA.  The index in
	// Global.BSA does not store offsets, only the size of the data.  Since
	// all data is packed without any padding, we need to accumulate the
	// sizes to figure out the offset of each file within Global.BSA.
	//
	for (DWORD i = 0; i < entryCount; ++i) {
		memset(&(pIndex[i]), 0, sizeof(pIndex[i]));
		memcpy(pIndex[i].Name, pFileIndex[i].Name, sizeof(pFileIndex[i].Name));

		pIndex[i].Offset = offset;
		pIndex[i].Size   = DWORD(pFileIndex[i].Size);

		offset += DWORD(pFileIndex[i].Size);
	}

	// Sort all of the file names according to stricmp's logic.  This will
	// allow for binary searches over the index without running afoul of the
	// several entries that are not in the correct order.
	for (long top = long(entryCount) - 1; top > 0; --top) {
		long highest = 0;
		for (long j = 1; j <= top; ++j) {
			if (stricmp(pIndex[highest].Name, pIndex[j].Name) < 0) {
				highest = j;
			}
		}
		if (highest != top) {
			GlobalIndex_t t = pIndex[highest];
			pIndex[highest] = pIndex[top];
			pIndex[top]     = t;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	LookUpResource()
//
//	Searches through index table for a specified file.  Assume that the given
//	name is actually in the table.  The table is mostly sorted, so a binary
//	search will usually locate the requested entry very fast.  Failing that,
//	do a linear search to handle the cases where things in the table are not
//	quite sorted (usually the file extension is backwards).
//
//	WARNING: Searching is case sensitive.  Global.BSA's index is not exactly
//	sorted, and searching via stricmp() only makes the problem worse.
//
DWORD ArenaLoader::LookUpResource(char name[])
{
	DWORD lo = 0;
	DWORD hi = m_IndexCount - 1;

	while (long(lo) <= long(hi)) {
		DWORD mid = (lo + hi) / 2;

		long result = stricmp(name, m_pGlobalIndex[mid].Name);
		if (result < 0) {
			hi = mid - 1;
		}
		else if (result > 0) {
			lo = mid + 1;
		}
		else {
			return mid;
		}
	}

	// Fallback case: If the binary search above fails, do a linear search.
	// A few of the entries in the file index are out of order, so binary
	// search cannot always succeed.  Here we can use case-insensitive testing,
	// since we brute-force testing every single file name.
	for (DWORD i = 0; i < m_IndexCount; ++i) {
		if (0 == stricmp(name, m_pGlobalIndex[i].Name)) {
			return i;
		}
	}

	return DWORD(-1);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadPalette()
//
//	Takes a 256-entry color palette, with each entry a 3-byte triplet, with
//	color values stored in reverse order.  This will expand each 3-byte
//	triple into a 32-bit word, fix the color ordering so blue is properly
//	stored in the low byte, and pad the alpha channel.
//
//	Although some color palettes and Windows COLORREF values store colors
//	bass ackwards with red in the low byte, everything else non-Motorola in
//	the world stores blue in the low byte, including Window bitmaps and
//	DirectX.
//
void ArenaLoader::LoadPalette(char filename[], DWORD palette[])
{
	char fullname[256];

	BYTE raw[256*3];

	sprintf(fullname, "%s%s", g_ArenaPath, filename);
	FILE *pFile = fopen(fullname, "rb");
	if (NULL != pFile) {
		// Skip the first 8 bytes at the start of the file.  The first four
		// bytes are 0x0000308, which is the size of the file.  The next four
		// bytes are 0x000B123 -- no idea what that value means (may be some
		// kind of version number from the original graphics editor, similar
		// to the values found in Daggerfall palettes).
		fseek(pFile, 8, SEEK_SET);
		fread(raw, 1, 3 * 256, pFile);
		fclose(pFile);

		for (DWORD i = 0; i < 256; ++i) {
			palette[i] = (DWORD(raw[i*3])<<16) | (DWORD(raw[i*3+1])<<8) | (DWORD(raw[i*3+2]));

			// For anything other than zero, make sure the alpha channel is
			// non-zero.  Index zero is used to indicate transparent zeroes for
			// many images (at least effects, characters (CFA files), and some
			// geometry (such as keyholes)).
			//
			// Render has been rewritten to work in colormapped space instead
			// of 32-bit ARGB, so the alpha channel does not matter anymore.
			//
			if (i > 0) {
				palette[i] |= 0xFF000000;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	GetPaletteARGB()
//
//	Returns the requested palette in ARGB format (blue value in low-order
//	byte).
//
DWORD* ArenaLoader::GetPaletteARGB(ArenaPalette_t select)
{
	if (DWORD(select) < DWORD(Palette_ArraySize)) {
		return m_Palette[select];
	}

	return NULL;
}


void ArenaLoader::GetPaletteARGB(ArenaPalette_t select, DWORD palette[])
{
	if (DWORD(select) < DWORD(Palette_ArraySize)) {
		memcpy(palette, m_Palette[select], 256 * sizeof(DWORD));
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	GetPaletteBlueFirst()
//
//	Returns the requested palette in 24-bit format, with blue in the low-order
//	byte.  This is the format used in TGA files.
//
void ArenaLoader::GetPaletteBlueFirst(ArenaPalette_t select, BYTE palette[])
{
	if (DWORD(select) < DWORD(Palette_ArraySize)) {
		DWORD *pSrc = m_Palette[select];

		// Make a copy of the palette, packing the 32-bit values into 24-bit
		// format.  Some image viewers have issues with TGA files that contain
		// 32-bit color palettes.
		for (DWORD q = 0; q < 256; ++q) {
			palette[q*3+0] = BYTE((pSrc[q]      ) & 0xFF);
			palette[q*3+1] = BYTE((pSrc[q] >>  8) & 0xFF);
			palette[q*3+2] = BYTE((pSrc[q] >> 16) & 0xFF);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	GetPaletteRedFirst()
//
//	Returns the requested palette in 24-bit format, with red in the low-order
//	byte.  This is the format used in GIF files.
//
void ArenaLoader::GetPaletteRedFirst(ArenaPalette_t select, BYTE palette[])
{
	if (DWORD(select) < DWORD(Palette_ArraySize)) {
		DWORD *pSrc = m_Palette[select];

		// Make a copy of the palette, packing the 32-bit values into 24-bit
		// format.  Some image viewers have issues with TGA files that contain
		// 32-bit color palettes.
		for (DWORD q = 0; q < 256; ++q) {
			palette[q*3+0] = BYTE((pSrc[q] >> 16) & 0xFF);
			palette[q*3+1] = BYTE((pSrc[q] >>  8) & 0xFF);
			palette[q*3+2] = BYTE((pSrc[q]      ) & 0xFF);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadFont()
//
//	Fonts are stored with 95 symbols, representing ASCII 33-127.  The first
//	95 bytes are a count of lines used for each symbol, followed by a bunch
//	of 16-bit values.  Each 16-bit word is a line of one symbol, in the form
//	of a bitmask indicating which pixels on that line should be colored.
//
//	There is no width value encoded in the font data.  This must be determined
//	by scanning each line of a particular symbol, locating the last (lowest)
//	bit on the line.
//
//	The symbol data is loaded into the given array as 96 symbols (adding one
//	for the space, so no special case is requiring to insert spaces into
//	text).  The lines are also stored as 16-bit values, leaving it to the
//	rendering code to scan the bits and determine which ones need to be set.
//
//
//	These are the font files found in the Arena directory:
//		ARENAFNT.DAT, height =  9
//		CHARFNT.DAT,  height =  8
//		FONT_A.DAT,   height = 11
//		FONT_B.DAT,   height =  6
//		FONT_C.DAT,   height = 14
//		FONT_D.DAT,   height =  7
//		FONT_S.DAT,   height =  5
//		FONT4.DAT,    height =  7
//		TEENYFNT.DAT, height =  8
//
void ArenaLoader::LoadFont(char filename[], DWORD fontHeight, FontElement_t symbols[])
{
	char fullname[256];
	sprintf(fullname, "%s%s", g_ArenaPath, filename);

	CMappedFile file;
	if (file.Open(fullname)) {
		BYTE* pCounts  = file.Address();
		WORD* pLines   = reinterpret_cast<WORD*>(pCounts + 95);
		DWORD height   = fontHeight;

		memset(symbols, 0, 96 * sizeof(FontElement_t));

		// Special case for the space.  There is no font symbol for it,
		// pad out a symbol for it based upon the font size.

		symbols[0].Width  = 4;
		symbols[0].Height = height + 1;

		for (DWORD i = 1; i < 96; ++i) {
			FontElement_t &element = symbols[i];
			element.Height = height;

			DWORD width = 0;

			for (DWORD lineNum = 0; lineNum < element.Height; ++lineNum) {
				element.Lines[lineNum] = *(pLines++);

				// Scan through this line of data to determine how many pixels
				// are required to draw it.  If this line is longer than any
				// others, update the width of the symbol.  Once done, we know
				// exactly how many pixels to render for this symbol, which
				// also is useful when computing the length of a full line of
				// text.
				//
				WORD mask = 0x8000;
				for (DWORD c = 0; c < 16; ++c) {
					if (0 != (element.Lines[lineNum] & mask)) {
						if (width < (c + 1)) {
							width = c + 1;
						}
					}

					mask = mask >> 1;
				}
			}

			// Pad out the width an extra pixel for inter-character spacing.
			width += 1;

			element.Width = width;
		}

		file.Close();
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	GetFont()
//
FontElement_t* ArenaLoader::GetFont(ArenaFont_t font)
{
	if (DWORD(font) < DWORD(Font_ArraySize)) {
		return m_FontList[font];
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadLightMap()
//
//	There are two lightmap files: "FOG.LGT" and "NORMAL.LGT".  Each of these
//	files is 13 * 256 bytes long, arranged as [13][256] where row[0][x] is
//	the normal color map.  The color values fad towards black (or fog) so
//	that [12][x] is the darkest version of the color.
//
//	However, this is reversed from what I'm used to (bigger numbers indicating
//	brighter lights), so loading the light maps will rearranged with [0][x]
//	being the darkest version of each color, [12][x] will be the brightest.
//
//	To further help the renderer, extra padding is added before and after
//	these lines.  This helps with some look-up indexing, and also is useful
//	for the dithering effect Arena does on pixel brightness to minimize the
//	obvious color banding along light gradients.
//
void ArenaLoader::LoadLightMap(char name[], BYTE buffer[], bool dump)
{
	char filename[256];
	sprintf(filename, "%s%s.LGT", g_ArenaPath, name);

	BYTE ramps[13][256];

	FILE *pFile = fopen(filename, "rb");

	if (NULL != pFile) {
		fread(ramps, 1, 13 * 256, pFile);
		fclose(pFile);
	}

	for (DWORD bright = 0; bright < 13; ++bright) {
		for (DWORD index = 0; index < 256; ++index) {
			buffer[((bright + 1) << 8) + index] = ramps[12-bright][index];
		}
	}

#if 1
	for (DWORD index = 0; index < 256; ++index) {
		buffer[( 0 << 8) + index] = ramps[12][index];
		buffer[(13 << 8) + index] = ramps[ 0][index];
		buffer[(14 << 8) + index] = ramps[ 0][index];
		buffer[(15 << 8) + index] = ramps[ 0][index];
	}
#else
	for (DWORD replicate = 0; replicate < 3; ++replicate) {
		for (DWORD index = 0; index < 256; ++index) {
			if (ramps[12][index] == ramps[0][index]) {
				buffer[(replicate << 8) + index] = ramps[12][index];
			}
			else {
				buffer[(replicate << 8) + index] = 0;
			}
		}
	}
#endif

	if (dump) {
		sprintf(filename, "%sImages\\%s.gif", g_ArenaPath, name);

		CGIFFile file;

		GIFFrameBuffer frame;
		frame.Width      = 256;
		frame.Height     = 13;
		frame.Pitch      = 256;
		frame.BufferSize = 13 * 256;
		frame.pBuffer    = reinterpret_cast<BYTE*>(ramps);
		frame.ColorMapEntryCount = 256;

		// GIF stores palettes with red in the low-order byte.
		GetPaletteRedFirst(Palette_Default, reinterpret_cast<BYTE*>(frame.ColorMap));

		file.Open(filename, false);
		file.SetCodeSize(8);
		file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
		file.InitOutput(256, 13);
		file.WriteImage(frame);
		file.Close();
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadData()
//
BYTE* ArenaLoader::LoadData(char name[], DWORD &dataSize)
{
	DWORD index = LookUpResource(name);

	dataSize = m_pGlobalIndex[index].Size;

	return m_pGlobalData + m_pGlobalIndex[index].Offset;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadSound()
//
//	Extracts sound data from a VOC file.  All of the sounds (all 76 of them)
//	are stored in the Creative/Soundblaster VOC format, as 8-bit data.  Each
//	of the sounds varies from 5 KHz to 11 KHz, with 8 KHz being the most
//	common.  The VOC format is a bit cumbersome to deal with, if only because
//	it is difficult to find details about this format.  Add to that all of
//	the glitches and bad samples in the audio data, and Arena's audio files
//	are a mess to deal with.
//
//	The loaded audio data will be stored in a dynamically allocated buffer,
//	the address of which is returned by this function.  The caller is then
//	responsible for freeing this buffer once it is no longer needed.
//
BYTE* ArenaLoader::LoadSound(char name[], DWORD &sampleCount, DWORD &sampleRate)
{
	DWORD index = LookUpResource(name);

	// There is a header at the beginning of the VOC data, but there is nothing
	// in it we care about (unless we want to sanity-check that the file format
	// is indeed a VOC file).
//	HeaderVOC_t *pHeader = reinterpret_cast<HeaderVOC_t*>(m_pGlobalData + m_pGlobalIndex[index].Offset);

	BYTE *pAudioStart = m_pGlobalData + m_pGlobalIndex[index].Offset + sizeof(HeaderVOC_t);

	sampleRate = 0;

	// After the VOC header, there will be a sequence of data packets, each
	// starting with a 32-bit header (usually 32 bits, but not always).  This
	// header indicates the type, and the size of the packet.  The lack of
	// available documentation on VOC files leaves it unknown if there is a
	// specific ordering to these packets, or if a robust looping mechanism
	// is needed to deal with them in arbitrary order.  They're always in a
	// consistent order for Arena's files, so doing this in order is adequate
	// for our purposes.


	// There may be a repeat packet at the start of data.  This only occurs
	// with DRUMS.VOC, which repeats 11 times.  We'll ignore the repeat
	// information, since this is the only file that uses it.
	//
	if (0x06 == pAudioStart[0]) {
		// Loop sound command:
		//   8-bit command: 0x06
		//  24-bit size
		//  16-bit loop count: zero-based count, add +1 to get correct count
		pAudioStart += 6;

		// There will also be an 0x07 "end repeat" command after the end of
		// the audio data.  We ignore that, since we're only reading the
		// audio data itself.  Nothing after the audio data will be looked at.
	}


	// Audio data packet size: This tells us how big the chunk of audio data
	// is.  Except for DRUMS.VOC, this is always the first packet (second in
	// DRUMS.VOC).
	//
	if (0x01 == pAudioStart[0]) {
		// Block format:
		//   8-bit ID == 0x01
		//  24-bit size value, includes next two bytes
		//   8-bit sample rate = 1000000 / (256 - value)
		//   8-bit packing flags
		//
		// The only thing needed here is the 16-bit size.  Subtract off 2
		// from the size, since we ignore the sample-rate and packing bytes.
		// None of Arena's files use packing.
		//
		sampleCount  = reinterpret_cast<WORD*>(pAudioStart+1)[0] - 2;

		sampleRate   = 1000000 / (256 - DWORD(pAudioStart[4]));

		// Bump up pointer to skip over all 6 bytes the header just viewed.
		pAudioStart += 6;
	}


	// Some clips start with the control sequence 0x57 0xFF.  No idea what this
	// does, and the sparse info on VOC files I could find makes no mention of
	// control values outside 0x00 - 0x09.
	//
	if (0x57 == pAudioStart[0]) {
		pAudioStart += 2;
		sampleCount -= 2;
	}


	// The 0x00 code is supposed to be a terminator.  Possibly it terminates
	// the header sequence before the start of actual audio samples.  But
	// there is no consistent usage of it.  Anywhere up to 10 bytes after the
	// first 0x00 will also be 0x00.  Sometimes there are only a few 0x00,
	// followed by a sequence of duplicated values, usually 0x7F, 0x80, or
	// 0x81.  Sometimes they are complete garbage.  Just throw all of them
	// away, even though this may discard some valid audio.  The "valid"
	// audio is always silence, so we're not losing anything by doing this.
	//
	if (0x00 == *pAudioStart) {
		pAudioStart += 11;
		sampleCount -= 11;
	}


	// Many of the sounds have an anomolous 0x73 8 bytes from the end of the
	// sound clip.  A few clips have it somewhere else in the last few bytes.
	// Scan backwards from the end of the clip and truncate the sound if this
	// value is seen.  Since it is always comes between a pair of 0x80 values
	// (except a couple cases where it is next to an 0x7F), it causes popping
	// at the end of these clips.  This cleans up these sounds significantly.
	//
	DWORD prevCount = sampleCount;
	for (DWORD backup = 1; backup <= 8; ++backup) {
		if (0x73 == pAudioStart[prevCount - backup]) {
			sampleCount = prevCount - backup;
		}
	}

	// One last special case is required for BASH.VOC, which has a bad sample
	// on the end.
	if (0x40 == pAudioStart[sampleCount-1]) {
		--sampleCount;
	}

	// After all of that clean-up, all of the sounds end with nice ramps down
	// to 0x80, plus or minus a bit or two.  There are some exceptions, such
	// as WOLF.VOC, which is truncated without dropping to full silence.  Other
	// files also have abrupt ends, and quite a few also still have garbage or
	// plateus at the beginning (e.g., a long sequence of 0x70, followed by
	// a sequence of 0x80).  Logic at a higher level is needed to explicitly
	// discard these bad starts.
	//
	// BACK1.VOC pops towards the end.
	// GHOST.VOC pops towards the end.
	// GROWL2.VOC has a pop at the very beginning.  Needs a ramp to soften impulse.
	// VAMPIRE.VOC has a pop near the end, but truncation may take too much off.

/*
	// Make a hex dump of the start and end of each clip to visually check
	// that all of the unknown control codes are gone.
	{
		char filename[256];
		sprintf(filename, "%sSounds\\sonic.txt", g_ArenaPath);
		static FILE *pfil = NULL;
		if (NULL == pfil) {
			pfil = fopen(filename, "w");
		}
		if (NULL != pfil) {
			fprintf(pfil, "%-14s %5d %5d %2d ", name, sampleRate, sampleCount, m_pGlobalIndex[index].Size - sampleCount);
			for (long i = 0; i < 20; ++i) {
				fprintf(pfil, " %02X", DWORD(pAudioStart[i]));
			}
			fprintf(pfil, " |");
			for (long j = -12; j < 2; ++j) {
				fprintf(pfil, " %02X", DWORD(pAudioStart[sampleCount+j]));
			}
			fprintf(pfil, "\n");
		}
	}
*/

	// Allocate a buffer big enough to contain the valid audio data, copy
	// those samples into place, and return that for the caller to use.

	BYTE *pBuffer = new BYTE[sampleCount];
	memcpy(pBuffer, pAudioStart, sampleCount);

	return pBuffer;
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpSounds()
//
//	All of the sound files are Creative Voice files.  Info about this file
//	format it hard to find, and incomplete.  What follows is the result of
//	experimentation.  But it works.
//
//	The sound data itself is 8-bit PCM, so we can write it out to WAV files
//	that are easy to play back.  I have no idea if this is linear PCM, A-law,
//	or mu-law.  If I'm interpreting the sample rates correctly, most sounds
//	are 8 KHz, though some vary from 5 KHz to 11 KHz.
//
//	The audio data itself is noisy, and tends to have glitches near the
//	end.  Care also needs to be taken to handle all of the audio packet
//	coding use by VOC files.  That might explain some of the pops in the
//	middle of a couple sound clips.
//
void ArenaLoader::DumpSounds(void)
{
	char  name[16];
	char  filename[256];

	MakeArenaDirectory("Sounds");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				name[c] = '\0';
				break;
			}
			else {
				name[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".VOC")) {

			sprintf(filename, "%sSounds\\%s.wav", g_ArenaPath, name);

			FILE *pFile = fopen(filename, "wb");

			DWORD sampleCount, sampleRate;
			BYTE *pBuffer = LoadSound(m_pGlobalIndex[index].Name, sampleCount, sampleRate);

			WAVEFORMATEX waveHeader;
			waveHeader.wFormatTag      = 1;
			waveHeader.nChannels       = 1;
			waveHeader.nSamplesPerSec  = sampleRate;
			waveHeader.nAvgBytesPerSec = sampleRate;
			waveHeader.nBlockAlign     = 1;
			waveHeader.wBitsPerSample  = 8;
			waveHeader.cbSize          = 0;

			long headerSize = sizeof(waveHeader);

			// Add 2 words for WAVE chunk, 1 for format tag, and 2 for data chunk.  The
			// initial 2 words for the RIFF chunk are not counted as part of the size.
			long fileSize = sampleCount + headerSize + (5 * sizeof(DWORD));

			long chunkSize = sampleCount;

			fwrite("RIFF", 4, 1, pFile);				// Write "RIFF"
			fwrite(&fileSize, 4, 1, pFile);				// Write Size of file with header, not counting first 8 bytes
			fwrite("WAVE", 4, 1, pFile);				// Write "WAVE"
			fwrite("fmt ", 4, 1, pFile);				// Write "fmt "
			fwrite(&headerSize, 4, 1, pFile);			// Size of header
			fwrite(&waveHeader, 1, headerSize, pFile);
			fwrite("data", 4, 1, pFile);				// Write "data"
			fwrite(&chunkSize, 4, 1, pFile);			// True length of sample data
			fwrite(pBuffer, 1, sampleCount, pFile);
			fclose(pFile);


/*
			// Dump the waveform as a big, fat TGA file.  Useful for spotting
			// the more obvious glitches in the audio data.

			TGAFileHeader header;
			memset(&header, 0, sizeof(header));
			header.ImageType = TGAImageType_BlackAndWhite;
			header.ImageSpec.ImageWidth  = WORD(sampleCount);
			header.ImageSpec.ImageHeight = 256;
			header.ImageSpec.PixelDepth  = 8;
			header.ImageSpec.ImageDescriptor = TGATopToBottom;

			BYTE *pImage = new BYTE[sampleCount * 256];

			memset(pImage, 0, sampleCount * 256);

			for (DWORD i = 0; i < sampleCount; ++i) {
				pImage[sampleCount * pBuffer[i] + i] = 255;
			}

			sprintf(filename, "Sounds\\%s.tga", name);

			pFile = fopen(filename, "wb");
			fwrite(&header, 1, sizeof(header), pFile);
			fwrite(pImage, 1, sampleCount*256, pFile);
			fclose(pFile);

			delete [] pImage;
*/

			delete [] pBuffer;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DecodeRLE()
//
//	Expands a run-length encoded chunk of data.  Much of the image data in
//	the various formats (IMG, CFA, DFA, CIF, ...) rely in part on run-length
//	encoding.  They often also use other encoding mechanisms to improve the
//	compression ratio by a smidgen.
//
//	FIXME: Look into this code some more.  Output data is sometimes larger
//	that expected.  Could be due to alignment issues -- a few extra bytes are
//	output per line, accumulating over all lines.  If so, need to account for
//	that, otherwise we'll be inadvertently truncating the data, since stopCount
//	will need to include the extra padding at end of each line.
//
//	ADDENDUM: looks like the problems were due to feeding in too large of a
//	<stopCount> value, but still need to verify
//
//		pRaw      - the input RLE data
//		pDecode   - where the unpacked data will be stored
//		stopCount - total number of bytes that should be present after decoding
//
DWORD DecodeRLE(BYTE pRaw[], BYTE pDecode[], DWORD stopCount)
{
	DWORD i = 0;
	DWORD o = 0;

	while (o < stopCount) {
		BYTE sample = pRaw[i++];

		// compressed packet
		if (sample & 0x80) {
			BYTE  value = pRaw[i++];
			DWORD count = DWORD(sample) - 0x7F;
			for (DWORD j = 0; j < count; ++j) {
				pDecode[o++] = value;
			}
		}
		else {
			DWORD count = DWORD(sample) + 1;
			for (DWORD j = 0; j < count; ++j) {
				pDecode[o++] = pRaw[i++];
			}
		}
	}

	return i;
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux7() ... Demux1()
//
//	What follow are a series of demuxing routines used by CFA files.  Image
//	data in CFAs assignes a unique index to each color used, then packed into
//	fewer bits.  For example, if there are only 16 unique colors, each of
//	those colors can be packed into 4-bit values, with two pixels packed into
//	one byte.
//
//	These routines take a packed sequence of bytes, and unpack them into
//	2 or more 8-bit values to make them easier to read.  The caller will still
//	need to dereference the resulting values to convert them into meaningful
//	color palette indices.
//
void Demux7(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0xFE) >> 1);
	pOut[1] = ((pIn[0] & 0x01) << 6) | ((pIn[1] & 0xFC) >> 2);
	pOut[2] = ((pIn[1] & 0x03) << 5) | ((pIn[2] & 0xF8) >> 3);
	pOut[3] = ((pIn[2] & 0x07) << 4) | ((pIn[3] & 0xF0) >> 4);
	pOut[4] = ((pIn[3] & 0x0F) << 3) | ((pIn[4] & 0xE0) >> 5);
	pOut[5] = ((pIn[4] & 0x1F) << 2) | ((pIn[5] & 0xC0) >> 6);
	pOut[6] = ((pIn[5] & 0x3F) << 1) | ((pIn[6] & 0x80) >> 7);
	pOut[7] = ((pIn[6] & 0x7F)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux6()
//
//	Decodes 6-bit compressed data.  In this format, four 6-bit pixels are
//	packed into three bytes.  Assumes that pIn points to 3 bytes, which are
//	then unpacked and the resulting 4 bytes written to pOut.
//
//	If the input contains less than 3 bytes of valid data, the pointer still
//	still needs to reference a 3-byte sequence, but the missing high bytes
//	should be padded with zeroes to permit all output values to be decoded
//	correctly.
//
void Demux6(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0xFC) >> 2);
	pOut[1] = ((pIn[0] & 0x03) << 4) | ((pIn[1] & 0xF0) >> 4);
	pOut[2] = ((pIn[1] & 0x0F) << 2) | ((pIn[2] & 0xC0) >> 6);
	pOut[3] = ((pIn[2] & 0x3F)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux5()
//
//	This is the 5-bit version of Demux...().  In this format, eight 5-bit
//	values are packed into 5 bytes.  The bits need to be reshuffled and
//	stored in the 8 bytes that pOut points to.
//
void Demux5(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0xF8) >> 3);
	pOut[1] = ((pIn[0] & 0x07) << 2) | ((pIn[1] & 0xC0) >> 6);
	pOut[2] = ((pIn[1] & 0x3E) >> 1);
	pOut[3] = ((pIn[1] & 0x01) << 4) | ((pIn[2] & 0xF0) >> 4);
	pOut[4] = ((pIn[2] & 0x0F) << 1) | ((pIn[3] & 0x80) >> 7);
	pOut[5] = ((pIn[3] & 0x7C) >> 2);
	pOut[6] = ((pIn[3] & 0x03) << 3) | ((pIn[4] & 0xE0) >> 5);
	pOut[7] = ((pIn[4] & 0x1F)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux4()
//
void Demux4(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = (pIn[0] & 0xF0) >> 4;
	pOut[1] = (pIn[0] & 0x0F);
	pOut[2] = (pIn[1] & 0xF0) >> 4;
	pOut[3] = (pIn[1] & 0x0F);
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux3()
//
void Demux3(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0xE0) >> 5);
	pOut[1] = ((pIn[0] & 0x1C) >> 2);
	pOut[2] = ((pIn[0] & 0x03) << 1) | ((pIn[1] & 0x80) >> 7);
	pOut[3] = ((pIn[1] & 0x70) >> 4);
	pOut[4] = ((pIn[1] & 0x0E) >> 1);
	pOut[5] = ((pIn[1] & 0x01) << 2) | ((pIn[2] & 0xC0) >> 6);
	pOut[6] = ((pIn[2] & 0x38) >> 3);
	pOut[7] = ((pIn[2] & 0x07)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux2()
//
void Demux2(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0xC0) >> 6);
	pOut[1] = ((pIn[0] & 0x30) >> 4);
	pOut[2] = ((pIn[0] & 0x0C) >> 2);
	pOut[3] = ((pIn[0] & 0x03)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	Demux1()
//
void Demux1(BYTE *pIn, BYTE *pOut)
{
	pOut[0] = ((pIn[0] & 0x80) >> 7);
	pOut[1] = ((pIn[0] & 0x40) >> 6);
	pOut[2] = ((pIn[0] & 0x20) >> 5);
	pOut[3] = ((pIn[0] & 0x10) >> 4);
	pOut[4] = ((pIn[0] & 0x08) >> 3);
	pOut[5] = ((pIn[0] & 0x04) >> 2);
	pOut[6] = ((pIn[0] & 0x02) >> 1);
	pOut[7] = ((pIn[0] & 0x01)     );
}


/////////////////////////////////////////////////////////////////////////////
//
//	CheckCFA()
//
//	Checks the header of a CFA file, extracting the resolution and number of
//	frames in the file.  Not all CFAs have the same number of frames, so it
//	is not safe to assume a given number of frames.
//
void ArenaLoader::CheckCFA(DWORD id, DWORD &width, DWORD &height, DWORD &frameCount)
{
	HeaderCFA_t *pHeader = reinterpret_cast<HeaderCFA_t*>(m_pGlobalData + m_pGlobalIndex[id].Offset);

	width      = pHeader->WidthUncompressed;
	height     = pHeader->Height;
	frameCount = pHeader->FrameCount;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadCFA()
//
//	Decodes a creature animation.  This will take the form of several frames
//	packed into a single buffer as a slideshow sequence.  The frames are
//	packed, without any extra data between them.
//
//	This function takes an array of frame buffers.  The images encoded in the
//	CFA are decoded, then copied one at a time into the array of frame buffers.
//
//	The CFA format is subjected to several levels of encoding.
//	 1) First off, the base palettized image is palettized again.  A look-up
//		table is stored at the beginning of the animation, containing only
//		those color indices that are used by the animation.
//	 2) Each re-palettized index value is packed (muxed) into as few bits as
//		possible (e.g., if there are only 7 distinct colors, each encoded
//		value is stored in 3 bits).  The bit data is packed up to 8-bit
//		alignment, so eight 3-bit values will be packed into 24 bits, filling
//		3 bytes.  If the image width is not a multiple of this value, extra
//		data is stored after the end of the line to pad it to the appropriate
//		multiple of bytes.  This is why the CFA header distinguishes between
//		the image's compressed and uncompressed widths.
//	 3) The resulting image data is subjected to run-length encoding.  Since
//		all CFAs contain a substantial amount of transparent black, this gives
//		most of the compression.
//
void ArenaLoader::LoadCFA(DWORD id, DWORD width, DWORD height, DWORD pitch, DWORD frameCount, BYTE *ppFrames[])
{
	BYTE *pBeginCFA = m_pGlobalData + m_pGlobalIndex[id].Offset;

	HeaderCFA_t *pHeader = reinterpret_cast<HeaderCFA_t*>(pBeginCFA);

	// Protect against requests for frames outside the bounds of the image.
	if (frameCount > DWORD(pHeader->FrameCount)) {
		return;
	}

	// The caller should have already called CheckCFA so it could allocate a
	// buffer of appropriate size.  Sanity-check those values.
	if ((width != pHeader->WidthUncompressed) || (height != pHeader->Height)) {
		return;
	}

	// Get the address where the look-up conversion table is located.  This is
	// used to map the packed color values back to useful palette values.
	//
	BYTE *pRemap = pBeginCFA + 0x4C;

	// Get the address where RLE-compressed data starts.  This will include the
	// remapping data stored at 0x4C.
	BYTE *pStart = pBeginCFA + pHeader->HeaderSize;

	// Create a line buffer on the stack.  Generously over-allocate it, since a
	// few extra bytes may be required while demuxing.
	BYTE *pEncoded = (BYTE*)alloca(width + 16);

	// Zero out the buffer.  Demuxing may require a few extra bytes, but as
	// long as they are all zeroes, the end result will have the demuxing
	// returning zeroes (transparent black) for the extra pixels.
	memset(pEncoded, 0, width + 16);

	// Demuxing will return 2-8 index values per pass.  We'll store them
	// here while translating them into actual color values.
	BYTE translate[8];

	// Allocate a worst-case buffer to hold the de-RLE'd data, since we need
	// to deal with padding for demux alignment.
	//
	// NOTE: We can't use m_pScratch here, since it is already in use by
	// higher level code.
	//
	BYTE *pDecomp = new BYTE[pHeader->WidthCompressed * height * DWORD(pHeader->FrameCount) * sizeof(DWORD) + width * 16];

	// The first step is to uncompress the run-length encoded data.  Once
	// this is done, the buffer will contain the bit-packed data, and will
	// also have a predictable size, so we can compute the start of each
	// packed line using pHeader->WidthCompressed.
	DecodeRLE(pStart, pDecomp, pHeader->WidthCompressed * height * DWORD(pHeader->FrameCount));

	// Byte offset into the bit-packed data to the beginning of the requested
	// frame.  Since the frames are packed one after another, we can iterate
	// over all of them without needing to update the offset between frames.
	DWORD offset = 0;

	for (DWORD frameNum = 0; frameNum < frameCount; ++frameNum) {

		// Pick the next frame buffer in the list.
		BYTE *pOut = ppFrames[frameNum];

		// Data needs to be decoded one line at a time.  The pixel samples are
		// normally packed into smaller byte groupings (e.g., 4 pixels packed into
		// 3 bytes), however, the uncompressed data may not be a multiple of the
		// packing size (for example, a width of 17).  So each line needs to be
		// decoded independently to avoid letting data wrap around and corrupt
		// other lines.  The extra space was already padded with zeroes, so when
		// we demux the packed bits, the extra samples also turn into zeroes.
		for (DWORD y = 0; y < height; ++y) {
			DWORD count = width;

			// Copy the line of data to a scratch buffer.  The extra space in the
			// line buffer has been padded with zeroes, which will prevent errors
			// from occurring when demuxing the last pixels on the line (which is
			// only relevant if the line length does not divide evenly with the
			// bytes-per-muxed grouping -- 3 for 6-bit, 5 for 5-bit).
			//
			memcpy(pEncoded, pDecomp + offset, pHeader->WidthCompressed);

			// Now, based upon how many bits are used to store each packed pixel,
			// we demux the bits into expanded 8-bit palette indices.  With the
			// exception of 8-bit pixels, we need to translate the demuxed values
			// into color indices using the values stored in the pRemap buffer.
			//
			if (pHeader->BitsPerPixel == 8) {
				for (DWORD x = 0; x < pHeader->WidthCompressed; ++x) {
					pOut[x] = pEncoded[x];
				}
			}
			else if (pHeader->BitsPerPixel == 7) {
				for (DWORD x = 0; x < DWORD((pHeader->WidthCompressed + 6) / 7); ++x) {
					Demux7(pEncoded + (x*7), translate);

					DWORD upTo = min(8, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*8+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 6) {
				for (DWORD x = 0; x < DWORD((pHeader->WidthCompressed + 2) / 3); ++x) {
					Demux6(pEncoded + (x*3), translate);

					DWORD upTo = min(4, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*4+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 5) {
				for (DWORD x = 0; x < DWORD((pHeader->WidthCompressed + 4) / 5); ++x) {
					Demux5(pEncoded + (x*5), translate);

					DWORD upTo = min(8, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*8+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 4) {
				for (DWORD x = 0; x < DWORD((pHeader->WidthCompressed + 1) / 2); ++x) {
					Demux4(pEncoded + (x*2), translate);

					DWORD upTo = min(4, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*4+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 3) {
				for (DWORD x = 0; x < DWORD((pHeader->WidthCompressed + 2) / 3); ++x) {
					Demux3(pEncoded + (x*3), translate);

					DWORD upTo = min(8, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*8+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 2) {
				for (DWORD x = 0; x < pHeader->WidthCompressed; ++x) {
					Demux2(pEncoded + x, translate);

					DWORD upTo = min(4, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*4+i] = pRemap[translate[i]];
					}
				}
			}
			else if (pHeader->BitsPerPixel == 1) {
				for (DWORD x = 0; x < pHeader->WidthCompressed; ++x) {
					Demux1(pEncoded + x, translate);

					DWORD upTo = min(8, count);
					count -= upTo;

					for (DWORD i = 0; i < upTo; ++i) {
						pOut[x*8+i] = pRemap[translate[i]];
					}
				}
			}

			// Move the offset to the start of the next compressed line.
			offset += pHeader->WidthCompressed;
			pOut   += pitch;
		}
	}

	SafeDeleteArray(pDecomp);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpCFA()
//
//	Dumps all of the character animations out to files.  All frames within
//	each CFA file are written to a single animated GIF.
//
void ArenaLoader::DumpCFA(void)
{
	char  name[16];
	char  filename[256];
	DWORD frameNum;

	MakeArenaDirectory("Creatures");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				name[c] = '\0';
				break;
			}
			else {
				name[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".CFA")) {
			CGIFFile file;

			BYTE *pBeginCFA = m_pGlobalData + m_pGlobalIndex[index].Offset;

			HeaderCFA_t *pHeader = reinterpret_cast<HeaderCFA_t*>(pBeginCFA);

			BYTE **ppFrames = reinterpret_cast<BYTE**>(alloca(pHeader->FrameCount * sizeof(BYTE*)));

			for (frameNum = 0; frameNum < pHeader->FrameCount; ++frameNum) {
				ppFrames[frameNum] = new BYTE[pHeader->WidthUncompressed * pHeader->Height];
			}

			LoadCFA(index, pHeader->WidthUncompressed, pHeader->Height, pHeader->WidthUncompressed, pHeader->FrameCount, ppFrames);

			GIFFrameBuffer frame;
			frame.Width      = pHeader->WidthUncompressed;
			frame.Height     = pHeader->Height;
			frame.Pitch      = pHeader->WidthUncompressed;
			frame.BufferSize = pHeader->WidthUncompressed * pHeader->Height;
			frame.pBuffer    = ppFrames[0];
			frame.ColorMapEntryCount = 256;

			// GIF stores palettes with red in the low-order byte.
			GetPaletteRedFirst(Palette_Default, reinterpret_cast<BYTE*>(frame.ColorMap));

			// Special case for ghosts and wraiths.  They are rendered using
			// palette tricks.  The "color values" are actual light index
			// values that are used to darken the background behind them.
			// To make them look reasonable outside the game, replace the low
			// color values with greyscale so they don't look like a riot of
			// colors.
			//
			if ((0 == strncmp(m_pGlobalIndex[index].Name, "GHOST", 5)) ||
				(0 == strncmp(m_pGlobalIndex[index].Name, "WRAITH", 6)))
			{
//				for (DWORD i = 1; i < 14; ++i) {
//					frame.ColorMap[i][0] = BYTE(19*(13 - i));
//					frame.ColorMap[i][1] = BYTE(19*(13 - i));
//					frame.ColorMap[i][2] = BYTE(19*(13 - i));
//				}
				for (DWORD i = 0; i < 14; ++i) {
					frame.ColorMap[i][0] = BYTE(130 - (10 * i));
					frame.ColorMap[i][1] = BYTE(130 - (10 * i));
					frame.ColorMap[i][2] = BYTE(130 - (10 * i));
				}
			}

			sprintf(filename, "%sCreatures\\%s.gif", g_ArenaPath, name);
			file.Open(filename, false);
			file.SetCodeSize(8);
			file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
			file.SetDelayTime(25);
			file.InitOutput(pHeader->WidthUncompressed, pHeader->Height);
			file.StartAnimation();
			for (frameNum = 0; frameNum < pHeader->FrameCount; ++frameNum) {
				frame.pBuffer = ppFrames[frameNum];
				file.WriteImage(frame);
			}
			file.FinishOutput();
			file.Close();

			for (frameNum = 0; frameNum < pHeader->FrameCount; ++frameNum) {
				SafeDeleteArray(ppFrames[frameNum]);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	CheckDFA()
//
void ArenaLoader::CheckDFA(DWORD id, DWORD &width, DWORD &height, DWORD &frameCount)
{
	HeaderDFA_t *pHeader = reinterpret_cast<HeaderDFA_t*>(m_pGlobalData + m_pGlobalIndex[id].Offset);

	width      = pHeader->Width;
	height     = pHeader->Height;
	frameCount = pHeader->FrameCount;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadDFA()
//
//	DFAs are a form of animation, but simpler than CFA.  Whereas CFAs have a
//	separate image per frame, DFAs store a single base frame with animation
//	changing a portion of it (e.g., the bartender sprite only animates the
//	hand).  However, some DFAs completely replace the image contents every
//	frame (the crystal ball and some guard sprites completely clear the image
//	for some frames).
//
//	The base frame is encoded using only RLE.  This is immediately followed
//	by a block of replacement values encoded as a byte offset + new color.
//
//
void ArenaLoader::LoadDFA(DWORD id, DWORD width, DWORD height, DWORD pitch, DWORD frameNum, BYTE *pResult)
{
	HeaderDFA_t *pHeader = reinterpret_cast<HeaderDFA_t*>(m_pGlobalData + m_pGlobalIndex[id].Offset);

	if ((width != pHeader->Width) || (height != pHeader->Height)) {
		return;
	}

	BYTE *pData  = m_pGlobalData + m_pGlobalIndex[id].Offset;
	BYTE *pStart = pData + sizeof(HeaderDFA_t);

	// Removing the RLE will provide the base image data.  Unlike CFAs,
	// nothing more is needed to make this frame useful.
	DWORD compressedSize = DecodeRLE(pStart, m_pScratch, width * height);

	assert(compressedSize == pHeader->CompressedSize);

	// Take the 12 bytes of the header, plus the size of the compressed
	// data.  This gives an offset to the start of the replacement values.
	DWORD offset = 12 + pHeader->CompressedSize;

	// Replacement data takes the form of chunks, each chunk containing
	// the changes required for a group of pixels.  Each frame (or block)
	// uses one or more chunks of replacement values.

	for (DWORD blockNum = 1; blockNum <= frameNum; ++blockNum) {
		DWORD chunkSize  = reinterpret_cast<WORD*>(pData+offset)[0];
		DWORD chunkCount = reinterpret_cast<WORD*>(pData+offset)[1];

		// If this not the frame we want, skip all of the chunks in it.
		// All changes are relative to the original frame, not to the
		// previous frame.
		if (blockNum < frameNum) {
			offset += 2 + chunkSize;
		}
		else {
			// Skip over the header.  We're now looking at the first
			// chunk of replacement colors.
			offset += 4;

			// Loop over each chunk.  This will contain a count of pixels,
			// and a byte offset relative to the start of the image, followed
			// immediately by a line of pixels that replace a chunk of
			// pixels in the previous image.
			for (DWORD chunkNum = 0; chunkNum < chunkCount; ++chunkNum) {
				DWORD replaceOffset = reinterpret_cast<WORD*>(pData+offset)[0];
				DWORD replaceCount  = reinterpret_cast<WORD*>(pData+offset)[1];
				offset += 4;
				for (DWORD i = 0; i < replaceCount; ++i) {
					m_pScratch[replaceOffset+i] = pData[offset++];
				}
			}
		}
	}

	// Once the modified frame has been established, blit the individual
	// lines into the destination buffer, accounting for the image's pitch.
	for (DWORD y = 0; y < height; ++y) {
		BYTE *pSrc = m_pScratch + (y * width);
		BYTE *pDst = pResult    + (y * pitch);
		for (DWORD x = 0; x < width; ++x) {
			pDst[x] = pSrc[x];
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpDFA()
//
void ArenaLoader::DumpDFA(void)
{
	char  name[16];
	char  filename[256];

	MakeArenaDirectory("Anims");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				name[c] = '\0';
				break;
			}
			else {
				name[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".DFA")) {

			HeaderDFA_t *pHeader = reinterpret_cast<HeaderDFA_t*>(m_pGlobalData + m_pGlobalIndex[index].Offset);

			BYTE *decode    = new BYTE[pHeader->Width * pHeader->Height];
			BYTE *duplicate = new BYTE[pHeader->Width * pHeader->Height];

			BYTE *pData  = m_pGlobalData + m_pGlobalIndex[index].Offset;
			BYTE *pStart = pData + sizeof(HeaderDFA_t);

			DWORD compressedSize = DecodeRLE(pStart, decode, pHeader->Width * pHeader->Height);

			assert(compressedSize == pHeader->CompressedSize);

			CGIFFile file;

			GIFFrameBuffer frame;
			frame.Width      = pHeader->Width;
			frame.Height     = pHeader->Height;
			frame.Pitch      = pHeader->Width;
			frame.BufferSize = pHeader->Width * pHeader->Height;
			frame.pBuffer    = decode;
			frame.ColorMapEntryCount = 256;

			// GIF stores palettes with red in the low-order byte.
			GetPaletteRedFirst(Palette_Default, reinterpret_cast<BYTE*>(frame.ColorMap));

			sprintf(filename, "%sAnims\\%s.gif", g_ArenaPath, name);
			file.Open(filename, false);
			file.SetCodeSize(8);
			file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
			file.SetDelayTime(25);
			file.InitOutput(pHeader->Width, pHeader->Height);
			file.StartAnimation();
			file.WriteImage(frame);


			DWORD offset = 12 + pHeader->CompressedSize;

			for (DWORD frameNum = 1; frameNum < pHeader->FrameCount; ++frameNum) {
				memcpy(duplicate, decode, pHeader->Width * pHeader->Height);

				// Chunk size is total number of bytes used for this compressed
				// frame.  Current offset + chunkSize + 2 = start of next frame.
				// After the last frame in the DFA, there will be an extra chunk
				// size word that is set to zero, which also denotes the end of
				// the animation sequence.
				//
				// This code uses the total frame count and the number of chunks
				// in each frame to process the animation, so chunk size will be
				// ignored.
				//
//				DWORD chunkSize  = reinterpret_cast<WORD*>(pData+offset)[0];
				DWORD chunkCount = reinterpret_cast<WORD*>(pData+offset)[1];

				offset += 4;

				for (DWORD chunkNum = 0; chunkNum < chunkCount; ++chunkNum) {
					DWORD replaceOffset = reinterpret_cast<WORD*>(pData+offset)[0];
					DWORD replaceCount  = reinterpret_cast<WORD*>(pData+offset)[1];
					offset += 4;
					for (DWORD i = 0; i < replaceCount; ++i) {
						duplicate[replaceOffset+i] = pData[offset++];
					}
				}

				frame.pBuffer = duplicate;

				file.WriteImage(frame);
			}

			file.FinishOutput();
			file.Close();

			SafeDeleteArray(duplicate);
			SafeDeleteArray(decode);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	CheckCIF()
//
void ArenaLoader::CheckCIF(char name[], DWORD frameNum, DWORD &width, DWORD &height, DWORD &offsetX, DWORD &offsetY)
{
	DWORD offset     = 0;
	DWORD frameIndex = 0;
	DWORD fileSize   = 0;
	BYTE* pFileData  = LoadFile(name, fileSize);

	while (offset < fileSize) {
		HeaderCIF_t *pHeader = reinterpret_cast<HeaderCIF_t*>(pFileData + offset);

		if (frameIndex == frameNum) {
			width   = pHeader->Width;
			height  = pHeader->Height;
			offsetX = pHeader->u1;
			offsetY = pHeader->u2;
			return;
		}

		offset     += pHeader->RecordSize + 12;
		frameIndex += 1;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadCIF()
//
//	A CIF contains several frames of data, which may or may not be animation.
//	This includes the player's weapon animations, different mouse pointers,
//	all unique keys from main-quest dungeons, character faces, and equipment
//	overlays for the inventory screen.
//
//	NOTE: The compression mechanism used for most of the images remains
//	unknown.  Only the weapon animations (RLE) and mouse pointers (uncompressed)
//	can presently be extracted.
//
void ArenaLoader::LoadCIF(char name[], DWORD width, DWORD height, DWORD frameNum, BYTE *pResult)
{
	DWORD offset     = 0;
	DWORD frameIndex = 0;
	DWORD fileSize   = 0;
	BYTE* pFileData  = LoadFile(name, fileSize);

	while (offset < fileSize) {
		HeaderCIF_t *pHeader = reinterpret_cast<HeaderCIF_t*>(pFileData + offset);

		if (frameIndex == frameNum) {
			assert(width  == pHeader->Width);
			assert(height == pHeader->Height);

			// Default to assuming we can copy uncompressed data straight
			// from the source buffer.
			BYTE* pSource = pFileData + offset + 12;

			// If the image is compressed, need to decompress it into a
			// temporary buffer first.
			if (0x0802 == pHeader->Flags) {

				DecodeRLE(pSource, m_pScratch, width * height);

				pSource = m_pScratch;
			}

			// FIXME: need to figure out the other compression options

			// Now blit the image into the destination buffer, accounting for
			// the buffer pitch.
			for (DWORD y = 0; y < height; ++y) {
				BYTE *pSrc = pSource + (y * width);
				BYTE *pDst = pResult + (y * (width + 1));
				for (DWORD x = 0; x < width; ++x) {
					pDst[x] = pSrc[x];
				}
			}
		}

		offset     += pHeader->RecordSize + 12;
		frameIndex += 1;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpCIF()
//
void ArenaLoader::DumpCIF(void)
{
	char  name[16];
	char  filename[256];

	MakeArenaDirectory("CIF");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				name[c] = '\0';
				break;
			}
			else {
				name[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".CIF")) {
			DWORD offset   = 0;
			DWORD frameNum = 0;
			DWORD fileSize = 0;

			BYTE* pFileData = LoadFile(m_pGlobalIndex[index].Name, fileSize);

			while (offset < fileSize) {

				HeaderCIF_t* pHeader   = reinterpret_cast<HeaderCIF_t*>(pFileData + offset);
				BYTE*        pBaseData = pFileData + offset + 12;

				if (0 == pHeader->Flags) {
					sprintf(filename, "%sCIF\\%s%02d.tga", g_ArenaPath, name, frameNum);

					TGAFileHeader header;
					memset(&header, 0, sizeof(header));
					header.ImageType = TGAImageType_ColorMapped;
					header.ColorMapType           = 1;
					header.ColorMapSpec.Length    = 256;
					header.ColorMapSpec.EntrySize = 24;
					header.ImageSpec.ImageWidth   = WORD(pHeader->Width);
					header.ImageSpec.ImageHeight  = WORD(pHeader->Height);
					header.ImageSpec.PixelDepth   = 8;
					header.ImageSpec.ImageDescriptor = TGATopToBottom;

					BYTE palette[3*256];
					GetPaletteBlueFirst(Palette_Default, palette);

					FILE *pFile = fopen(filename, "wb");
					if (NULL != pFile) {
						fwrite(&header, 1, sizeof(header), pFile);
						fwrite(palette, 1, 3 * 256, pFile);
						fwrite(pBaseData, 1, pHeader->Width * pHeader->Height, pFile);
						fclose(pFile);
					}
				}
				else if (0x0802 == pHeader->Flags) {
					BYTE* decode = new BYTE[pHeader->Width * pHeader->Height];

					DWORD d = 0;

					for (DWORD j = 0; j < pHeader->RecordSize; ++j) {
						BYTE code = pBaseData[j];
						if (0x80 & code) {
							DWORD count = (DWORD(code) & 0x7F) + 1;
							++j;
							BYTE value = pBaseData[j];
							for (DWORD i = 0; i < count; ++i) {
								if (d < DWORD(pHeader->Width * pHeader->Height)) {
									decode[d++] = value;
								}
							}
						}
						else {
							DWORD count = DWORD(code) + 1;
							for (DWORD i = 0; i < count; ++i) {
								if (d < DWORD(pHeader->Width * pHeader->Height)) {
									decode[d++] = pBaseData[++j];
								}
							}
						}
					}

					sprintf(filename, "%sCIF\\%s%02d.tga", g_ArenaPath, name, frameNum);

					TGAFileHeader header;
					memset(&header, 0, sizeof(header));
					header.ImageType = TGAImageType_ColorMapped;
					header.ColorMapType           = 1;
					header.ColorMapSpec.Length    = 256;
					header.ColorMapSpec.EntrySize = 24;
					header.ImageSpec.ImageWidth   = WORD(pHeader->Width);
					header.ImageSpec.ImageHeight  = WORD(pHeader->Height);
					header.ImageSpec.PixelDepth   = 8;
					header.ImageSpec.ImageDescriptor = TGATopToBottom;

					BYTE palette[3*256];
					GetPaletteBlueFirst(Palette_Default, palette);

					FILE *pFile = fopen(filename, "wb");
					if (NULL != pFile) {
						fwrite(&header, 1, sizeof(header), pFile);
						fwrite(palette, 1, 3 * 256, pFile);
						fwrite(decode, 1, pHeader->Width * pHeader->Height, pFile);
						fclose(pFile);
					}

					delete [] decode;
				}
				else {
				}

				offset   += pHeader->RecordSize + 12;
				frameNum += 1;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	The following data table holds a list of IMG files that do not have
//	headers.  Need to catch any of these files and explicitly fill in the
//	width/height values for them.  This also tells us whether to expect a
//	header at the beginning of the file, or if the image data starts at the
//	beginning of the file.
//

struct IMGExceptions_t
{
	char* Name;
	DWORD Width;
	DWORD Height;
};


IMGExceptions_t g_IMGExceptions[] =
{
	{ "ARENARW.IMG",  16, 16 },
	{ "CITY.IMG",     16, 11 },
	{ "DUNGEON.IMG",  14,  8 },
	{ "DZTTAV.IMG",   32, 34 },
	{ "NOCAMP.IMG",   25, 19 },
	{ "NOSPELL.IMG",  25, 19 },
	{ "P1.IMG",      320, 53 },
	{ "POPTALK.IMG", 320, 77 },
	{ "S2.IMG",      320, 36 },
	{ "SLIDER.IMG",  289,  7 },
	{ "TOWN.IMG",      9, 10 },
	{ "UPDOWN.IMG",    8, 16 },
	{ "VILLAGE.IMG",   8,  8 }
};


/////////////////////////////////////////////////////////////////////////////
//
//	CheckIMG()
//
//	Locates a named IMG file, fills in the width/height, and returns the
//	important flag that indicates whether the file has a header.  LoadIMG()
//	needs to know whether there is a header so it can locate the actual
//	image data.
//
void ArenaLoader::CheckIMG(char name[], DWORD &width, DWORD &height, DWORD &offsetX, DWORD &offsetY, bool &hasHeader)
{
	// Some of the IMG files are raw data with no headers.  If this file is
	// one of those, we need to extract the data from a custom table that
	// contains the missing info.
	for (DWORD i = 0; i < ArraySize(g_IMGExceptions); ++i) {
		if (0 == stricmp(name, g_IMGExceptions[i].Name)) {
			width     = g_IMGExceptions[i].Width;
			height    = g_IMGExceptions[i].Height;
			hasHeader = false;
			return;
		}
	}

	DWORD fileSize   = 0;
	BYTE* pFileData  = LoadFile(name, fileSize);

	HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(pFileData);

	width     = pHeader->Width;
	height    = pHeader->Height;
	offsetX   = pHeader->OffsetX;
	offsetY   = pHeader->OffsetY;
	hasHeader = true;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadIMGPalettized()
//
//	Base function used by LoadIMG() and DumpIMG() to access the image data,
//	and return it in uncompressed format.
//
//	NOTE: This function knows nothing of the headerless IMG files.  Only call
//	it if CheckIMG() indicates that the IMG has a header.
//
BYTE* LoadIMGPalettized(BYTE *pFileData, DWORD fileSize, DWORD width, DWORD height, BYTE *pResult)
{
	HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(pFileData);

	if ((width != pHeader->Width) || (height != pHeader->Height)) {
		return NULL;
	}

	DWORD pixelCount = width * height;

	// Pad the return buffer with transparent black.
	memset(pResult, 0, pixelCount);

	bool compressed = (pixelCount != pHeader->RecordSize);

	BYTE *pData = pFileData + 12;

	if (pHeader->Flags & IMG_FLAG_PIXELCOUNT) {
		pData += 2;
	}

	if (compressed) {
		// FIXME: compression format remains unknown at this time
	}
	else {
		memcpy(pResult, pData, width * height);
	}

	// If there is a palette, it will start 3 * 256 bytes from the end of the
	// file.
	if (pHeader->Flags & IMG_FLAG_PALETTE) {
		return pFileData + fileSize - 0x300;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadIMG()
//
//	Loads IMG files.  There are three main varieties.  Some IMG files have
//	no header, requiring the app to know the exact resolution of the image.
//	This also includes wall/floor textures, but those are handled differently
//	since they are used differently by the renderer.
//
//	The second major type of IMG is raw, uncompressed image data, but with a
//	header that indicates the size.
//
//	The third variant is compressed, using a technique currently unknown.
//	These images are normally full-frame images, like character screens and
//	the main dungeon splash pages (Fang Lair, Dagoth-Ur, etc.).
//
DWORD* ArenaLoader::LoadIMG(char name[], DWORD width, DWORD height, DWORD pitch, bool hasHeader, BYTE *pResult)
{
	DWORD fileSize   = 0;
	BYTE* pFileData  = LoadFile(name, fileSize);

	// Temporary palette to use in case the image contains a custom palette.
	DWORD palette[256];

	// Default to using the standard color palette.
	DWORD *pPalette = NULL;
	
	if (hasHeader) {
		BYTE *pPalette24 = LoadIMGPalettized(pFileData,fileSize, width, height, m_pScratch);

		if (NULL != pPalette24) {
			// Fix up the palette that is stored in the image.  These palettes
			// are stored in the order [red,green,blue], so the byte order needs
			// to be fixed so they can be used.  Additionally, these are 6-bit
			// VGA graphics palettes, so the bits need to be shifted upwards to
			// turn them into 8-bit values.
			//
			for (DWORD i = 0; i < 256; ++i) {
				palette[i]	= (DWORD(pPalette24[i*3+2]) <<  2)
							| (DWORD(pPalette24[i*3+1]) << 10)
							| (DWORD(pPalette24[i*3+0]) << 18);
			}

			pPalette = palette;
		}

	}

	// Special case for those few IMG files that have no headers.  These contain
	// only the color data, so no decompression is required.  But the caller
	// will need to explicitly provide the width/height of the image data.
	else {
		memcpy(m_pScratch, pFileData, width * height);
	}


	// Need to copy the resulting image to the destination buffer one line at
	// a time to account for differing pitch values.
	for (DWORD y = 0; y < height; ++y) {
		BYTE *pLine  = pResult + (y * pitch);
		BYTE *pStart = m_pScratch + (y * width);

		for (DWORD x = 0; x < width; ++x) {
			pLine[x] = pStart[x];
		}
	}

	// If the image has a custom palette, store it in a dynamically allocated
	// buffer that the caller should cache with the image.  This palette will
	// be required to properly display the image.  Only full-frame images have
	// custom palettes.
	if (NULL != pPalette) {
		DWORD *pTemp = new DWORD[256];

		memcpy(pTemp, pPalette, 256 * sizeof(DWORD));

		return pTemp;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpIMG()
//
//	Dumps a standard IMG file, which (usually) has a header, and contains a
//	single frame of image data.
//
void ArenaLoader::DumpIMG(char filename[])
{
/*
	DWORD resID = LookUpResource(filename);

	char savename[64];
	strcpy(savename, "Images\\");
	DWORD i = 0;
	DWORD c = DWORD(strlen(savename));
	while (m_pGlobalIndex[resID].Name[i] != '.') {
		savename[c++] = m_pGlobalIndex[resID].Name[i++];
	}
	strcpy(savename+c, ".tga");

	HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(m_pGlobalData + m_pGlobalIndex[resID].Offset);

	BYTE *pData = m_pGlobalData + m_pGlobalIndex[resID].Offset + 12;

	FILE *pFile = fopen(savename, "wb");

	BYTE palette[256 * 3];

	GetPaletteBlueFirst(Palette_Default, palette);

	DWORD frameSize = pHeader->Width * pHeader->Height;

	TGAFileHeader header;
	memset(&header, 0, sizeof(header));
	header.ColorMapType              = 1;
	header.ImageType                 = TGAImageType_ColorMapped;
	header.ColorMapSpec.Length       = 256;
	header.ColorMapSpec.EntrySize    = 24;
	header.ImageSpec.ImageWidth      = pHeader->Width;
	header.ImageSpec.ImageHeight     = pHeader->Height;
	header.ImageSpec.PixelDepth      = 8;
	header.ImageSpec.ImageDescriptor = TGATopToBottom;

	fwrite(&header, 1, sizeof(header), pFile);
	fwrite(palette, 1, 3 * 256, pFile);
	fwrite(pData, 1, frameSize, pFile);
	fclose(pFile);
*/

	DWORD fileSize   = 0;
	BYTE* pFileData  = LoadFile(filename, fileSize);

	DWORD width, height, offsetX, offsetY;
	bool  hasHeader;

	CheckIMG(filename, width, height, offsetX, offsetY, hasHeader);

	BYTE palette[3*256];

	// GIF stores palettes with red in the low-order byte.
	GetPaletteRedFirst(Palette_Default, palette);

	BYTE *pPixels = new BYTE[width * height];

	if (hasHeader) {
		HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(pFileData);

		// FIXME: Remove this when the code knows how to deal with compressed
		// IMG files.  Until then, there is no need to dump solid black GIFs
		// off to disk for the hapless user to sort through.
		if (pHeader->RecordSize != width * height) {
			return;
		}

		BYTE *pPalette24 = LoadIMGPalettized(pFileData, fileSize, width, height, pPixels);

		if (NULL != pPalette24) {
			// Fix up the palette that is stored in the image.  Since GIF images
			// store color palettes with the same bass ackwards byte ordering,
			// we don't need to reverse the byte order here.  However, we still
			// need to convert the 6-bit VGA palettes embedded in the images to
			// 8-bit values.
			//
			for (DWORD i = 0; i < 3*256; ++i) {
				palette[i] = pPalette24[i] << 2;
			}
		}
	}

	// Special case for those few IMG files that have no headers.  These contain
	// only the color data, so 
	else {
		memcpy(pPixels, pFileData, width * height);
	}

	char savename[256];
	sprintf(savename, "%sImages\\", g_ArenaPath);
	DWORD i = 0;
	DWORD c = DWORD(strlen(savename));
	while (filename[i] != '.') {
		savename[c++] = filename[i++];
	}
	strcpy(savename+c, ".gif");

	CGIFFile file;

	GIFFrameBuffer frame;
	frame.Width      = width;
	frame.Height     = height;
	frame.Pitch      = width;
	frame.BufferSize = width * height;
	frame.pBuffer    = pPixels;
	frame.ColorMapEntryCount = 256;

	memcpy(frame.ColorMap, palette, 3 * 256);

	file.Open(savename, false);
	file.SetCodeSize(8);
	file.SetGlobalColorMap(256, palette);
	file.InitOutput(width, height);
	file.WriteImage(frame);
	file.FinishOutput();
	file.Close();

	delete [] pPixels;
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpIMGRAW()
//
//	Alternate version of DumpIMG() that was used to extract image data from
//	files while trying to figure out how the data was stored.  Assumes the
//	image has no header, and needs a palette explicitly provided.
//
//	Now that the g_IMGExceptions[] array has been populated, DumpIMG can
//	handle all of those files.
//
//	This function is obsolete, but kept for future testing.
//
void ArenaLoader::DumpIMGRAW(char filename[], DWORD width, DWORD height, DWORD palette32[])
{
	DWORD resID = LookUpResource(filename);

	char savename[64];
	strcpy(savename, "Images\\");
	DWORD i = 0;
	DWORD c = DWORD(strlen(savename));
	while (m_pGlobalIndex[resID].Name[i] != '.') {
		savename[c++] = m_pGlobalIndex[resID].Name[i++];
	}
	strcpy(savename+c, ".tga");

	BYTE *pData = m_pGlobalData + m_pGlobalIndex[resID].Offset;

	FILE *pFile = fopen(savename, "wb");

	BYTE palette[256 * 3];

	for (i = 0; i < 256; ++i) {
		palette[i*3+0] = BYTE(palette32[i*3]      );
		palette[i*3+1] = BYTE(palette32[i*3] >>  8);
		palette[i*3+2] = BYTE(palette32[i*3] >> 16);
	}

	DWORD frameSize = width * height;

	TGAFileHeader header;
	memset(&header, 0, sizeof(header));
	header.ColorMapType              = 1;
	header.ImageType                 = TGAImageType_ColorMapped;
	header.ColorMapSpec.Length       = 256;
	header.ColorMapSpec.EntrySize    = 24;
	header.ImageSpec.ImageWidth      = WORD(width);
	header.ImageSpec.ImageHeight     = WORD(height);
	header.ImageSpec.PixelDepth      = 8;
	header.ImageSpec.ImageDescriptor = TGATopToBottom;

	fwrite(&header, 1, sizeof(header), pFile);
	fwrite(palette, 1, 3 * 256, pFile);
	fwrite(pData, 1, frameSize, pFile);
	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpIMGFAT()
//
//	Alternate version of DumpIMG for images containing their own palette.
//	This was used for testing different image formats.  Few apply to this
//	case, since virtually all IMG files with palettes are full-frame and
//	compressed.
//
//	This function is obsolete, but kept for future testing.
//
void ArenaLoader::DumpIMGFAT(char filename[])
{
	DWORD resID = LookUpResource(filename);

	char savename[64];
	strcpy(savename, "Images\\");
	DWORD i = 0;
	DWORD c = DWORD(strlen(savename));
	while (m_pGlobalIndex[resID].Name[i] != '.') {
		savename[c++] = m_pGlobalIndex[resID].Name[i++];
	}
	strcpy(savename+c, ".tga");

	HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(m_pGlobalData + m_pGlobalIndex[resID].Offset);

	BYTE *pData    = m_pGlobalData + m_pGlobalIndex[resID].Offset + 12;
	BYTE *pPalette = pData + pHeader->RecordSize;

	DWORD frameSize = pHeader->Width * pHeader->Height;

	FILE *pFile = fopen(savename, "wb");

	BYTE palette[256 * 3];

	for (i = 0; i < 256; ++i) {
		palette[i*3+0] = pPalette[i*3+2] << 2;
		palette[i*3+1] = pPalette[i*3+1] << 2;
		palette[i*3+2] = pPalette[i*3+0] << 2;
	}

	TGAFileHeader header;
	memset(&header, 0, sizeof(header));
	header.ColorMapType              = 1;
	header.ImageType                 = TGAImageType_ColorMapped;
	header.ColorMapSpec.Length       = 256;
	header.ColorMapSpec.EntrySize    = 24;
	header.ImageSpec.ImageWidth      = pHeader->Width;
	header.ImageSpec.ImageHeight     = pHeader->Height;
	header.ImageSpec.PixelDepth      = 8;
	header.ImageSpec.ImageDescriptor = TGATopToBottom;

	fwrite(&header, 1, sizeof(header), pFile);
	fwrite(palette, 1, 3 * 256, pFile);
	fwrite(pData, 1, frameSize, pFile);
	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpImages()
//
//	Oh, the joy of dumping IMG files.  Some IMG files are images, some aren't.
//	Some images don't have the .IMG extension.  Some IMG files have no header
//	struct, while others have one of several structs.  As yet, I've determined
//	no rhyme or reason to IMG files.  It's pretty much a matter of checking
//	a hex dump to figure out how to decompress the image, requiring a priori
//	knowledge of what the IMG actually is before trying to use it.
//
void ArenaLoader::DumpImages(void)
{
	MakeArenaDirectory("Images");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".IMG")) {
			// Ignore textures with .IMG extensions
			if (0 == (m_pGlobalIndex[index].Size % (64 * 64))) {
				continue;
			}

			DumpIMG(m_pGlobalIndex[index].Name);
		}
	}

	BYTE *lightmap = new BYTE[18*256];
	LoadLightMap("FOG", lightmap, true);
	LoadLightMap("NORMAL", lightmap, true);
	delete [] lightmap;

/*
	The following images are compressed.  Until I figure out how the compression
	scheme works, these images cannot be loaded.

	Compression seems to work as a sequence of byte codes, with each code possibly
	taking up more than one byte.  It may even be a bit stream, which will require
	tracking decode state to know which bits to read, and how to interpret them.

	Decoding starts with a current pixel value of zero.  Each bit sequence instructs
	the codec to output one or more pixels based on previous pixels and a delta
	value.  This is not RLE, nor is it any standard flavor of delta coding I've
	seen before.

	For example, take the first few types of CHARSTAT.IMG: 86 83 90 00 40 E3

	86 means "subtract 6 from the current pixel value", so it outputs 250 (or -6)

	83 means "subtract 3 from the current pixel value", so the next output value
	is 247

	90 00 means "take the current pixel value, add zero, and replicate it 7 times,
	so it outputs 247 seven more times.  Changing 90 to 98 causes it to output 15
	more values.  Changing 00 to 01 causes it to output seven values by replicating
	the two previous values (so it outputs an alterating sequence of 250 and 247)

	and somehow, it also outputs a 244, so it may also duplicate the "subtract 3"
	from the previous command

	40 means "subtract 4 from the current pixel value", so it outputs 240

	Changing these bits doesn't change things in ways I would expect.  It's almost
	like the bit parser is toggling some of the input bits before it computes the
	count or delta values.  Or maybe it's just because I don't yet understand how
	the codec logic works.


	0ARGHELM.IMG
	1ARGHELM.IMG
	ACCPREJT.IMG
	BKFACESF.IMG
	BLAKMRSH.IMG
	BONUS.IMG
	CHARBK00.IMG - CHARBK07.IMG
	CHARSTAT.IMG
	CHRBKF00.IMG - CHRBKF07.IMG
	COLOSSUS.IMG
	CRYPT.IMG
	DAGOTHUR.IMG
	DITHER.IMG		<-- 800-byte ramp from 0x1 to 0x8, possibly used for lighting
	DITHER2.IMG		<-- 800-byte ramp from 0x8 to 0x1
	ELSWEYR.IMG
	EQUIP.IMG
	EQUIPB.IMG
	FANGLAIR.IMG
	FLIZ0.IMG
	FLIZ1.IMG
	FLIZ2.IMG
	FORM1.IMG - FORM15.IMG
	FOROOZ1.IMG
	FPANTS.IMG
	FRSHIRT.IMG
	FSSHIRT.IMG
	GOLD.IMG
	GROVE.IMG
	HAMERFEL.IMG
	HIGHROCK.IMG
	HISTORY.IMG
	HOUSE.IMG
	IMPERIAL.IMG
	INTRO01.IMG - INTRO09.IMG
	INTRO1.IMG - INTRO7.IMG		<-- these appear to be identical files
	LABRINTH.IMG
	MENU.IMG
	MIRKWOOD.IMG
	MLIZ0.IMG
	MLIZ1.IMG
	MLIZ2.IMG
	MOROWIND.IMG
	MPANTS.IMG
	MRSHIRT.IMG
	MSSHIRT.IMG
	NEGOTBUT.IMG
	NEWEQUIP.IMG
	NEWMENU.IMG
	NEWOLD.IMG
	NEW_CTR.IMG
	NEW_LFT.IMG
	NEW_RT.IMG
	NOEXIT.IMG
	PAGE2.IMG
	PARCH.IMG
	POPUP.IMG
	POPUP2.IMG
	POPUP8.IMG
	QUOTE.IMG
	SCROLL01.IMG
	SCROLL02.IMG
	SCROLL03.IMG
	SKULL.IMG
	SKYRIM.IMG
	SPELLBK.IMG
	STARTGAM.IMG
	SUMERSET.IMG
	TAMRIEL.IMG
	TEST.IMG
	TOP.IMG
	TOWER.IMG
	VALNWOOD.IMG
	YESNO.IMG
*/
}


/////////////////////////////////////////////////////////////////////////////
//
//	ScanIMGs()
//
//	Utility function for figuring out the image formats.  Creates a dump
//	file named "scanIMG.txt" that lists all IMG files, sizes, and format
//	info.
//
void ArenaLoader::ScanIMGs(void)
{
	FILE *pFile = fopen("scanIMG.txt", "w");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".IMG")) {

			HeaderIMG_t *pHeader = reinterpret_cast<HeaderIMG_t*>(m_pGlobalData + m_pGlobalIndex[index].Offset);

			DWORD actualSize = m_pGlobalIndex[index].Size;
			DWORD pixelCount = pHeader->Width * pHeader->Height;
			DWORD compressed = pixelCount != pHeader->RecordSize;
			DWORD expectedSize = 12 + pixelCount;

			// Ignore textures with .IMG extensions
			if (0 == (actualSize % (64 * 64))) {
				continue;
			}

			DWORD width  = pHeader->Width;
			DWORD height = pHeader->Height;

			if ((width > 320) || (height > 200)) {
				width  = 0;
				height = 0;
			}

			bool normal = (expectedSize == actualSize);

			bool sizeEmbed = (pixelCount == pHeader->UncompressedSize);

			bool hasPal = (actualSize == DWORD(12 + pHeader->RecordSize + 0x300));

			bool paletteFlag = (0 != (pHeader->Flags & IMG_FLAG_PALETTE));

			fprintf(pFile, "%14s: %3d %3d %04X ", m_pGlobalIndex[index].Name, width, height, pHeader->Flags);

			fprintf(pFile, !normal ? "odd " : "    ");

			fprintf(pFile, compressed ? "comp " : "     ");

			fprintf(pFile, hasPal ? "pal " : "    ");

			fprintf(pFile, paletteFlag ? "palF " : "     ");

			fprintf(pFile, sizeEmbed ? "emb " : "    ");

			fprintf(pFile, "\n");
		}
	}

	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadWallTexture()
//
//	This is used to extract block textures (walls, floors, ceilings).  These
//	are all 64x64 images, without any encoding or packing (or header info,
//	for that matter).  These may be IMG resouces, or they many be SET files.
//	In the case of SET files, several 64x64 blocks are packed into the file.
//	The offset parameter should be set to a multiple of 64x64 bytes, so that
//	the code can skip to the requested frame in the set.
//
void ArenaLoader::LoadWallTexture(DWORD index, BYTE *pPixels, DWORD offset)
{
	BYTE *pStart = m_pGlobalData + m_pGlobalIndex[index].Offset + offset;

	// There is no coding/compression with this data.  Just blit the lines
	// to the destination buffer.  This version is hard-coded for the
	// renderer, which stores the textures 64x64.
	for (DWORD y = 0; y < 64; ++y) {
		BYTE *pLine = pPixels + (y * 64);
		for (DWORD x = 0; x < 64; ++x) {
			pLine[x] = *(pStart++);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	LoadWater()
//
//	Water and lava textures are stored in RCI files.  These are located in
//	Arena's main directory.  They are uncompressed, without any headers at
//	all -- just 5 frames of 320x100 pixel image data.
//
void ArenaLoader::LoadWater(char name[], DWORD frameNum, BYTE *pPixels)
{
	char filename[256];
	sprintf(filename, "%s%s", g_ArenaPath, name);

	FILE *pFile = fopen(filename, "rb");

	if (NULL != pFile) {
		fseek(pFile, frameNum * 320 * 100, SEEK_SET);
		
		fread(pPixels, 1, 320 * 100, pFile);

		fclose(pFile);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// The following data tables are used to decode INF files.  While the INF
// files sitting the Arena directory are plain text (presumably patched, since
// the have slightly different contents from the copies in Global.BSA), those
// INF files in Global.BSA are encoded with a simple bit-hashing technique.
//
// Data encoded in the high nibble of each byte uses a rotating look-up table.
// The index into the code table changes for each byte, repeating every 16
// bytes.  This rotating offset is stored in g_RowShift[].  The second part
// of decoding the high nibble is to then apply a look-up into a 2D translation
// table using the byte's address / 16, and the 4-bit value of the high nibble.
//
// This is demonstrated in the DeobfuscateINF() function below.
//
// UPDATE: Apparently these tables can be resolved down to a single 256-byte
// array that is simply XORed against the encoded INF data to unencode it.
// I have not investigated that further, so I cannot comment.

DWORD g_RowShift[16] = {
	0,  9, 7, 14, 3, 14, 5, 12,
	1, 10, 7, 14, 4, 15, 6, 12
};


// Note that half of the table entries are missing.  The missing entries are
// the ones that would be used to encode ASCII values 128-255.  None of the
// INF files use any of those values, nor do any of the fonts have symbols
// defined in that range.  So it's safe to say this code won't ever see one
// of those invalid values, and if it did, Arena would probably crash if it
// tried to process the file.
//
char g_InverseCodeTableHigh[16][20] = {
"        67452301",
"        76543210",
"01234567        ",
"10325476        ",
"23016745        ",
"32107654        ",
"45670123        ",
"54761032        ",
"67452301        ",
"76543210        ",
"        01234567",
"        10325476",
"        23016745",
"        32107654",
"        45670123",
"        54761032"
};


// The low nibble of each byte serves as a index into a look-up table.
// The row of the table to use is based on the byte's offset, modulo 16.
// The encoding table was experimentally determined, and is stored in
// g_CodeTableLow[][].  To derive the reverse translation table, call
// MakeInverseCodeTable() one time.  Then g_InverseCodeTableLow[][]
// can be used to decode the low 4 bits of the bit stream.
//
char g_CodeTableLow[16][20] =
{
	"AB89EFCD23016745",  // [address % 16] == 0
	"CDEF89AB45670123",
	"0123456789ABCDEF",
	"0123456789ABCDEF",
	"DCFE98BA54761032",  // [address % 16] == 4
	"EFCDAB8967452301",
	"EFCDAB8967452301",
	"0123456789ABCDEF",
	"23016745AB89EFCD",  // [address % 16] == 8
	"45670123CDEF89AB",
	"89ABCDEF01234567",
	"89ABCDEF01234567",
	"54761032DCFE98BA",  // [address % 16] == 12
	"67452301EFCDAB89",
	"67452301EFCDAB89",
	"89ABCDEF01234567"
};

// This will be filled in by MakeInverseCodeTable().
//
DWORD g_InverseCodeTableLow[16][16];


// For readability purposes, the code tables were filled in with text.
// So those hex digits need to be translated to their corresponding
// 4-bit numerical values.
//
DWORD HexCharToInt(char c)
{
	if ((c >= 'A') && (c <= 'F')) {
		return char(c - 'A' + 10);
	}

	return char(c - '0');
}


// Decoding the low four bits experimentally derived the foreward translation
// table.  To perform the inverse translation, this function needs to be called
// first to fill in the g_InverseCodeTableLow[][] table.
//
void ArenaLoader::MakeInverseCodeTable(void)
{
	for (DWORD y = 0; y < 16; ++y) {
		for (DWORD x = 0; x < 16; ++x) {
			DWORD trans = HexCharToInt(g_CodeTableLow[y][x]);

			g_InverseCodeTableLow[y][trans] = x;
		}
	}
}


// This function decodes the encoded text.  It relies upon an inverse translation
// table, which is filled in by MakeInverseCodeTable(), so that function needs to
// be called before attempting to decode any text.
//
// Separate table look-up operations are used on the low and high nibbles of each
// byte.  The low 4 bits may be decoded by a simple table look-up, whereas the
// high four bits require a translation of one array index.
//
// The obfuscation mechanism is cyclic, repeating every 16 bytes for the low
// nibble, and every 256 bytes for the high nibble.  Therefore, the whole text
// chunk should be decoded at one time so DeobfuscateINF() can derive the hashing
// values from each byte's position in the text buffer.
//
// The code therefore decodes one byte at a time, splitting off the high and low
// bits for separate processing.  Once translated, the bits are combined and
// written back into the text buffer.
//
void ArenaLoader::DeobfuscateINF(BYTE text[], DWORD count)
{
	for (DWORD idx = 0; idx < count; ++idx) {
		DWORD encoded = DWORD(text[idx]);
		DWORD lobits  = (encoded     ) & 0xF;
		DWORD hibits  = (encoded >> 4) & 0xF;
		DWORD rownum  = (idx >> 4) & 0xF;
		DWORD colnum  = (idx     ) & 0xF;

		// The low 4 bits are decoded by a simple table look-up operation.
		lobits = g_InverseCodeTableLow[colnum][lobits];

		// The high 4 bits are subjected to a rotation that repeats every
		// 16 bytes.  This requires using one table to compute an index
		// into the second table.
		DWORD index  = (g_RowShift[colnum] + rownum) & 0xF;

		// Using the massaged index, the actual table look-up can be performed
		// to decode the high 4 bits.
		hibits = HexCharToInt(g_InverseCodeTableHigh[index][hibits]);

		// Then the low and high bits are muxed together again and written
		// back into the text buffer.
		DWORD muxen = (hibits << 4) | lobits;

		text[idx] = BYTE(muxen);
	}
}




/////////////////////////////////////////////////////////////////////////////
//
//	DumpBSAIndex()
//
//	Dumps a text file containing a list of all files in Global.BSA, along
//	with their size and offset relative to the start of the file.
//
void ArenaLoader::DumpBSAIndex(void)
{
	char filename[256];
	sprintf(filename, "%sGlobalIndex.txt", g_ArenaPath);

	FILE *pFile = fopen(filename, "w");
	if (NULL != pFile) {
		fprintf(pFile, "       Offset   Size   File Name\n");
		fprintf(pFile, "      -------- ------ -----------\n");

		for (DWORD i = 0; i < m_IndexCount; ++i) {
			fprintf(pFile, "%4d: 0x%06X 0x%04X %s\n",
				i,
				m_pGlobalIndex[i].Offset,
				m_pGlobalIndex[i].Size,
				m_pGlobalIndex[i].Name);
		}

		fclose(pFile);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpInfo()
//
//	Dumps all of the INF files out to text files.  INF are encoded using a
//	bit hashing (or apparently a simple XOR pattern), so they need to be
//	decoded before writing to a file.
//
//	WARNING: INF files in the Arena directory are not encoded, so logic would
//	need to know where an INF is located before applying any decoding to the
//	data before writing it to a text file.
//
void ArenaLoader::DumpInfo(void)
{
	MakeArenaDirectory("Info");

	char  name[16];
	char  filename[256];

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				name[c] = '\0';
				break;
			}
			else {
				name[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".INF")) {

			BYTE *pScratch = new BYTE[m_pGlobalIndex[index].Size];
			memcpy(pScratch, m_pGlobalData + m_pGlobalIndex[index].Offset, m_pGlobalIndex[index].Size);

			DeobfuscateINF(pScratch, m_pGlobalIndex[index].Size);

			sprintf(filename, "%sInfo\\%s.txt", g_ArenaPath, name);

			FILE *pFile = fopen(filename, "wb");

			fwrite(pScratch, 1, m_pGlobalIndex[index].Size, pFile);

			fclose(pFile);

			delete [] pScratch;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	SubHexDump()
//
//	Dumps a range of binary data to a file in hex-dump format.
//
void SubHexDump(FILE *pFile, BYTE *pData, DWORD dataLength)
{
	for (DWORD pos = 0; pos < dataLength; pos += 16) {
		int count = ((dataLength - pos) >= 16) ? 16 : (dataLength - pos);
		int j;

		BYTE *pBuf = pData + pos;

		fprintf(pFile, "    %04x:", pos);

		for (j = 0; j < count; ++j) {
			fprintf(pFile, " %02X", DWORD(pBuf[j]));

			if (7 == j) {
				fprintf(pFile, " ");
			}
		}

		for (j = count; j < 16; ++j) {
			fprintf(pFile, "   ");

			if (7 == j) {
				fprintf(pFile, " ");
			}
		}

		fprintf(pFile, "   ");

		for (j = 0; j < count; ++j) {
			if (isprint(pBuf[j])) {
				fprintf(pFile, "%c", pBuf[j]);
			}
			else {
				fprintf(pFile, ".");
			}
		}

		fprintf(pFile, "\n");
	}
}



/////////////////////////////////////////////////////////////////////////////
//
//	DumpMaps()
//
//	This is just experimental code.
//
//	Map (MIF) files are a form of RIFF (AVI) file format.  Data is broken
//	up into levels, which are subdivided into sub-chunks.  All this code is
//	split up the RIFF chunks and do a hex dump of each.  Although a guess can
//	be made to how the decoded data is used (given the very simplistic physical
//	model use for Arena levels), it remains unknown how to decode the MIF data,
//	or what the exact bit interpretation would be for the decoded data.
//
void ArenaLoader::DumpMap(char resname[], char prefix[])
{
	DWORD mapSize = 0;
	BYTE* pData   = LoadFile(resname, mapSize);

	char message[256], filename[256];

	sprintf(filename, "%sMaps\\%s.txt", g_ArenaPath, prefix);
	FILE *pFile = fopen(filename, "w");
	if (NULL != pFile) {
		DWORD offset = 0;

		while (offset < mapSize) {
			DWORD fourcc    = *reinterpret_cast<DWORD*>(pData + offset);
			WORD  chunkSize = *reinterpret_cast<WORD*>(pData + offset + 4);


			// first 2 bytes of MAP# and FLOR == 2 * width * height
			// which may imply decoded size is 16 bits per block

			if (MAKEFOURCC('M','H','D','R') == fourcc) {
				offset += 6;

				DWORD width  = pData[offset+21];
				DWORD height = pData[offset+23];

				fprintf(pFile, "MHDR: %d bytes (%dx%d)\n", chunkSize, width, height);

				SubHexDump(pFile, pData + offset, chunkSize);

				offset += chunkSize;
			}
			else if (MAKEFOURCC('L','E','V','L') == fourcc) {
				offset += 6;

				fprintf(pFile, "LEVL: %d bytes\n", chunkSize);
			}
			else if (MAKEFOURCC('N','A','M','E') == fourcc) {
				offset += 6;

				DWORD copySize = min(chunkSize, ArraySize(message) - 1);

				memcpy(message, pData + offset, copySize);
				message[copySize] = '\0';

				fprintf(pFile, "  NAME: \"%s\"\n", message);

				offset += chunkSize;
			}
			else if (MAKEFOURCC('I','N','F','O') == fourcc) {
				offset += 6;

				DWORD copySize = min(chunkSize, ArraySize(message) - 1);

				memcpy(message, pData + offset, copySize);
				message[copySize] = '\0';

				fprintf(pFile, "  INFO: \"%s\"\n", message);

				offset += chunkSize;
			}
			else if (MAKEFOURCC('N','U','M','F') == fourcc) {
				offset += 6;

				DWORD copySize = min(chunkSize, ArraySize(message) - 1);

				memcpy(message, pData + offset, copySize);
				message[copySize] = '\0';

				fprintf(pFile, "  NUMF: #%d, %d bytes\n", DWORD(pData[offset]), chunkSize);

				offset += chunkSize;
			}
			else {
				char label[5];
				label[0] = pData[offset+0];
				label[1] = pData[offset+1];
				label[2] = pData[offset+2];
				label[3] = pData[offset+3];
				label[4] = '\0';

				fprintf(pFile, "  %s: %d bytes\n", label, chunkSize);

				offset += 6;

				SubHexDump(pFile, pData + offset, chunkSize);

				offset += chunkSize;
			}
		}

		fclose(pFile);
	}
}


void ArenaLoader::DumpMaps(void)
{
	MakeArenaDirectory("Maps");

	char prefix[16];

	DumpMap("START.MIF", "START");

	for (DWORD index = 0; index < m_IndexCount; ++index) {
		for (DWORD c = 0; c < 13; ++c) {
			if (m_pGlobalIndex[index].Name[c] == '.') {
				prefix[c] = '\0';
				break;
			}
			else {
				prefix[c] = m_pGlobalIndex[index].Name[c];
			}
		}

		char *extension = strstr(m_pGlobalIndex[index].Name, ".");
		if (0 == stricmp(extension, ".MIF")) {
			DumpMap(m_pGlobalIndex[index].Name, prefix);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
//	DumpWalls()
//
//	Dumps all wall/floor/ceiling textures to a set of GIF files.  There are
//	a large number of these textures, so a bunch of them will be packed into
//	each GIF file to make previewing them easier.  Also writes the name of
//	the texture below the image, along with the index of the image within its
//	SET file (always zero if it is an IMG file with only one texture).
//
//	This code is stupid.  Since wall textures have no header, it simply looks
//	for all files that are evenly divisible by 64x64 pixels.  This is helped
//	a bit since textures can have either .IMG or .SET extensions to their
//	filenames, but not all .IMG files are textures.  Need to check the size
//	and skip anything that does not evenly divide by 64x64 pixels.
//
//	Note that the default color palette is always used.  This appears to be
//	the palette used for virtually all images (except possibly snowy, dreary,
//	etc. textures).
//
void ArenaLoader::DumpWalls(void)
{
	// How many textures to pack into each GIF file.
	DWORD gridWidth   = 8;
	DWORD gridHeight  = 8;

	// Amount of space to leave between each texture within the file.
	DWORD padX        = 4;
	DWORD padY        = 14;

	// How large does the image buffer need to be to contain these textures.
	DWORD frameWidth  = gridWidth  * (64 + padX);
	DWORD frameHeight = gridHeight * (64 + padY);
	DWORD frameSize   = frameWidth * frameHeight;
	BYTE* pBuffer     = new BYTE[frameSize];
	char  filename[256];

	MakeArenaDirectory("Walls");

	memset(pBuffer, 7, frameSize);

	GIFFrameBuffer frame;
	frame.Width      = frameWidth;
	frame.Height     = frameHeight;
	frame.Pitch      = frameWidth;
	frame.BufferSize = frameWidth * frameHeight;
	frame.pBuffer    = pBuffer;
	frame.ColorMapEntryCount = 256;

	// GIF stores palettes with red in the low-order byte.
	GetPaletteRedFirst(Palette_Default, reinterpret_cast<BYTE*>(frame.ColorMap));

	DWORD x = 0;
	DWORD y = 0;
	DWORD frameNum = 1;

	char base[64];
	char label[64];

	FontElement_t* pFont = GetFont(Font_4);

	// Loop over every file packed into Global.BSA.
	for (DWORD i = 0; i < m_IndexCount; ++i) {

		char *extension = strstr(m_pGlobalIndex[i].Name, ".");

		// Only .IMG and .SET files may be textures.
		bool possible = (0 == stricmp(extension, ".IMG")) || (0 == stricmp(extension, ".SET"));

		// Pick out any file that has a multiple of 64x64 bytes.
		if (possible && (0 == (m_pGlobalIndex[i].Size % 0x1000))) {

			// Pick out the first part of the file name, minus the extension.
			// There isn't quite enough space to add both the file extension
			// and the texture's index within the file.  Since the extension
			// is always either IMG or SET, the index value is more useful.
			for (DWORD q = 0; q < 16; ++q) {
				if (m_pGlobalIndex[i].Name[q] == '.') {
					base[q] = '\0';
					break;
				}
				else {
					base[q] = m_pGlobalIndex[i].Name[q];
				}
			}

			// How many textures are in this file.
			DWORD count = m_pGlobalIndex[i].Size / 0x1000;

			BYTE *pIn = m_pGlobalData + m_pGlobalIndex[i].Offset;

			// This is something of a cumbersome loop.  It takes each texture
			// in the file, writing it to some place within the current image.
			// If we've filled all of the spots in the image buffer, write the
			// data out to a GIF file, then reset the state back to being an
			// empty image.
			//
			for (DWORD num = 0; num < count; ++num) {

				BYTE *pOut = pBuffer + (x * (64 + padX)) + 2 + (y * frameWidth * (64 + padY)) + (2 * frameWidth);

				for (DWORD dy = 0; dy < 64; ++dy) {
					memcpy(pOut, pIn, 64);
					pIn  += 64;
					pOut += frameWidth;
				}

				// Write a string that indicates the file name and the
				// texture's index within the file.  This makes it easy to
				// consult the resulting GIF file to pick out images to use
				// in putting together sample levels.
				if (count > 1) {
					sprintf(label, "%s[%d]", base, num);
				}
				else {
					strcpy(label, base);
				}

				BYTE *pTarget = pOut + frameWidth;

				// Copy all characters of the string into the image file,
				// just below the texture data.  Given the way we've formatted
				// the string, it won't contain more than 11 characters.
				for (DWORD cn = 0; cn < 11; ++cn) {
					char sym = label[cn];
					if ((sym >= 32) && (sym <= 127)) {
						FontElement_t &element = pFont[sym - 32];

						BYTE *pTC   = pTarget;

						for (DWORD cz = 0; cz < element.Width; ++cz) {
							pTC[cz] = 0;
						}
						pTC += frameWidth;

						for (DWORD cy = 0; cy < element.Height; ++cy) {
							WORD mask = 0x8000;
							for (DWORD cx = 0; cx < element.Width; ++cx) {
								pTC[cx] = (element.Lines[cy] & mask) ? 7 : 0;

								mask = mask >> 1;
							}
							pTC += frameWidth;
						}

						pTarget += element.Width;
					}
					else {
						break;
					}
				}

				x += 1;

				// When we fill up the current row, reset the numbers to the
				// start of the next row.
				if (x >= gridWidth) {
					x  = 0;
					y += 1;
				}

				// If this means we've filled all rows in the current image,
				// dump it out to a file, then reset everything back to
				// being an empty image buffer.
				if (y >= gridHeight) {
					x = 0;
					y = 0;

					sprintf(filename, "%sWalls\\dump%02d.gif", g_ArenaPath, frameNum++);

					CGIFFile file;
					file.Open(filename, false);
					file.SetCodeSize(8);
					file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
					file.InitOutput(frameWidth, frameHeight);
					file.WriteImage(frame);
					file.FinishOutput();
					file.Close();

					memset(pBuffer, 7, frameSize);
				}
			}
		}
	}

	// Once we're done, we again need to dump the image to a file to
	// save the few remaining images sitting in the buffer.

	sprintf(filename, "%sWalls\\dump%02d.gif", g_ArenaPath, frameNum);

	CGIFFile file;
	file.Open(filename, false);
	file.SetCodeSize(8);
	file.SetGlobalColorMap(256, reinterpret_cast<BYTE*>(frame.ColorMap));
	file.InitOutput(frameWidth, frameHeight);
	file.WriteImage(frame);
	file.FinishOutput();
	file.Close();

	delete [] pBuffer;
}


/////////////////////////////////////////////////////////////////////////////
//
//	ExtractFile()
//
void ArenaLoader::ExtractFile(char filename[])
{
	DWORD resID = LookUpResource(filename);

	FILE *pFile = fopen(filename, "wb");
	fwrite(m_pGlobalData + m_pGlobalIndex[resID].Offset, 1, m_pGlobalIndex[resID].Size, pFile);
	fclose(pFile);
}


/////////////////////////////////////////////////////////////////////////////
//
//	HexDump()
//
void ArenaLoader::HexDump(char filename[], char path[], bool rightToLeft)
{
	MakeArenaDirectory("Hex");

	char textname[256];
	if (path[0] == '\0') {
		sprintf(textname, "%s%s%s.txt", g_ArenaPath, "Hex\\", filename);
	}
	else {
		sprintf(textname, "%s%s%s.txt", g_ArenaPath, path, filename);
	}

	DWORD resID = LookUpResource(filename);

	DWORD dumpSize = m_pGlobalIndex[resID].Size;

	FILE *pFile = fopen(textname, "w");

	if (NULL != pFile) {
		fprintf(pFile, "Dump: start 0x%08X, length 0x%08X:  [%s]\n",
			m_pGlobalIndex[resID].Offset,
			m_pGlobalIndex[resID].Size,
			rightToLeft ? "right-to-left" : "left-to-right");

		for (DWORD pos = 0; pos < dumpSize; pos += 16) {
			int count = ((dumpSize - pos) >= 16) ? 16 : (dumpSize - pos);
			int j;

			BYTE *pBuf = m_pGlobalData + pos + m_pGlobalIndex[resID].Offset;

			fprintf(pFile, "%06x:", pos);

			if (rightToLeft) {
				for (j = 15; j >= count; --j) {
					fprintf(pFile, "   ");

					if (8 == j) {
						fprintf(pFile, " ");
					}
				}

				for (j = count - 1; j >= 0; --j) {
					fprintf(pFile, " %02X", DWORD(pBuf[j]));

					if (8 == j) {
						fprintf(pFile, " ");
					}
				}

				fprintf(pFile, "   ");

				for (j = 15; j >= count; --j) {
					fprintf(pFile, " ");
				}

				for (j = count - 1; j >= 0; --j) {
					if (isprint(pBuf[j])) {
						fprintf(pFile, "%c", pBuf[j]);
					}
					else {
						fprintf(pFile, ".");
					}
				}
			}
			else {
				for (j = 0; j < count; ++j) {
					fprintf(pFile, " %02X", DWORD(pBuf[j]));

					if (7 == j) {
						fprintf(pFile, " ");
					}
				}

				for (j = count; j < 16; ++j) {
					fprintf(pFile, "   ");

					if (7 == j) {
						fprintf(pFile, " ");
					}
				}

				fprintf(pFile, "   ");

				for (j = 0; j < count; ++j) {
					if (isprint(pBuf[j])) {
						fprintf(pFile, "%c", pBuf[j]);
					}
					else {
						fprintf(pFile, ".");
					}
				}

			}

			fprintf(pFile, "\n");
		}

		fclose(pFile);
	}
}


