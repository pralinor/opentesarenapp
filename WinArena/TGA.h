/*////////////////////////////////////////////////////////////////////////////

  Copyright © Pinnacle Systems Inc.  1999
  Developer Use Permitted

  TGA file definitions: v3.0

  Last Updated: May 6, 1999

  This header file defines the structures, bit flags, and constants which are
  used to define a TGA file.  If an application is going to use these structures
  to format data for transfers to and from files, then the structures must be
  "packed" so that no extra bytes are introduced into the structures to force
  word alignment of the fields.  Otherwise an application will have to read and
  write each structure one field at a time.


  For this code, it is assumed that type "char" is 8 bits, type "short" is
  16 bits, and type "long" is 32 bits.

/*////////////////////////////////////////////////////////////////////////////


// Prevent multiple inclusion of this header file.
#if !defined(APP_TgaFile_h)
#define APP_TgaFile_h


// Use the pack pragma to pack all structure entries to byte alignment,
// preventing any extra bytes from being inserted into the structures
// by the compiler.
//
// If the compiler in use does not recognize this pragma, consult its
// documentation to determine the notation required to achieve.
// Some versions of GNU C will require use of the "__attribute__ ((packed))"
// declaration to force the required packing.
//
#pragma pack(push, normalPacking)
#pragma pack(1)


// The following constants define the types of image data supported by
// versions 1.0 and 2.0 of the TGA specification.
#define TGAImageType_NoImageData			 0
#define TGAImageType_ColorMapped			 1
#define TGAImageType_TrueColor				 2
#define TGAImageType_BlackAndWhite			 3
#define TGAImageType_RleColorMapped			 9
#define TGAImageType_RleTrueColor			10
#define TGAImageType_RleBlackAndWhite		11

#define	TGAImageType_RleDeltaHuffman		32	/* OBSOLETE: run-length, delta, Huffman	*/
#define	TGAImageType_RleDeltaHuffmanBlock	33	/* OBSOLETE: run-length, delta, Huffman, in block format */

// These image types require information in a v3.0 extension area in order
// to make use of the image data in the file.  If a v3.0 extension area is
// not present, then the image data cannot be decoded.
#define TGAImageType_ExtendedTrueColor		34
#define TGAImageType_ExtendedBlackAndWhite	35
#define TGAImageType_ExtendedSubSampled		36


// The two following bit masks are used to indicate the orientation of the
// image data contained in the TGA file.  These bits are stored in the
// image descriptor field of the file header.
#define TGATopToBottom						0x20
#define TGARightToLeft						0x10


// This 18-byte string (including the '\0' terminator) is contained in the
// file footer to indicate that the file is a v2.0 or later TGA file.
#define TGASignature						"TRUEVISION-XFILE."	


// The TGA file header is located at the beginning of a TGA file.
// It contains information defining the dimensions, pixel size, and
// color map (if present) of the image data.
// This structure should be exactly 18 bytes in length.
//
typedef struct tagTGAFileHeader {
	unsigned char		IDLength;
	unsigned char		ColorMapType;
	unsigned char		ImageType;
	struct {
		unsigned short	FirstEntryIndex;
		unsigned short	Length;
		unsigned char	EntrySize;
	} ColorMapSpec;
	struct {
		unsigned short	XOrigin;
		unsigned short	YOrigin;
		unsigned short	ImageWidth;
		unsigned short	ImageHeight;
		unsigned char	PixelDepth;
		unsigned char	ImageDescriptor;
	} ImageSpec;
} TGAFileHeader;


// The developer directory starts with a 16-bit value that indicates
// the number of developer tags that are contained in the directory.
// This value is immediately followed by the indicated number of tags,
// with each tag being defined by the following structure:
//
typedef struct tagTGADeveloperDirectory {
	unsigned short		Tag;		// unique ID tag assigned to developers
	unsigned long		Offset;		// location of data relative to start of file
	unsigned long		Size;		// size of the developer data, in bytes
} TGADeveloperDirectory;


// The extension areas of a TGA file define aspect ratios and gamma values
// by using two 16-bit values: a numerator followed by a denominator.  The
// decimal equivalent of this value is determined by dividing the numerator
// by the denominator.
//
typedef struct tagTGAFraction {
	unsigned short	Numerator;
	unsigned short	Denominator;
} TGAFraction;


// These values define the different values of the AttributesType field of
// the v2.0 extension area.  These values indicate whether alpha data is
// contained in the image data, whether that data may be used, and whether
// the color components have been pre-multiplied with the alpha values.
//
#define TGAAttribute_NoAlpha				0		// no alpha data in file
#define TGAAttribute_AlphaUndefinedIgnore	1		// alpha data present but undefined; it may be ignored
#define TGAAttribute_AlphaUndefinedRetain	2		// alpha data present but undefined; it should be retained
#define TGAAttribute_AlphaPresent			3		// alpha data present and usable
#define TGAAttribute_AlphaPremultiplied		4		// alpha data present; image data premultiplied by alpha


// The following structure defines a version 2.0 extension area.  Its size is
// exactly 495 bytes long (as stored in the ExtensionSize field).  If the size
// field contains a larger value, then a newer version of the extension area
// is stored in the file.
//		v2.0 ExtensionSize = 495
//		v3.0 ExtensionSize = 592
//
// Unused fields should be set to binary zeroes (except SoftwareVersion.Letter,
// which should be set to binary 0x20 (space ' ')).
//
typedef struct tagTGAExtension2Area {
	unsigned short		ExtensionSize;				// 495 for v2.0 or 592 for v3.0
	char				AuthorName[41];
	char				AuthorComments[4][81];
	struct {
		unsigned short	Month;
		unsigned short	Day;
		unsigned short	Year;
		unsigned short	Hour;
		unsigned short	Minute;
		unsigned short	Second;
	} TimeStamp;
	char				JobName[41];
	struct {
		unsigned short	Hours;
		unsigned short	Minutes;
		unsigned short	Seconds;
	} JobTime;
	char				SoftwareID[41];
	struct {
		unsigned short	Number;
		char			Letter;
	} SoftwareVersion;
	unsigned long		KeyColor;
	TGAFraction			PixelAspectRatio;
	TGAFraction			Gamma;
	unsigned long		ColorCorrectionOffset;		// color correction table, relative to start of file
	unsigned long		PostageStampOffset;			// postage stamp location, relative to start of file
	unsigned long		ScanLineOffset;				// scan line table location, relative to start of file
	unsigned char		AttributesType;				// defines type of alpha data present in the file
} TGAExtension2Area;


// Bit flags which are defined in the ExtensionAttributes field of the
// v3.0 extension area.
#define TGAExtFlag_SubsampledData		0x0001
#define TGAExtFlag_PackedSubsampled		0x0002
#define TGAExtFlag_SubsampleCositedX	0x0004
#define TGAExtFlag_SubsampleCositedY	0x0008
#define TGAExtFlag_ClampPrecision		0x0010
#define TGAExtFlag_PackedComponents		0x0020


// Types of color space data presently defined for TGA files.
#define TGAColorSpace_Undefined			0		// unknown or application-specific color space
#define TGAColorSpace_BlackAndWhite		1
#define TGAColorSpace_RGB				2
#define TGAColorSpace_YCbCr				3
#define TGAColorSpace_YPbPr				4


// The following structure defines a v3.0 extension area.  Note that this
// structure contains all of the v2.0 extension area fields.  The size of
// this structure is 592 bytes (as stored in the Ext2.ExtensionSize field).
//
typedef struct tagTGAExtension3Area {
	TGAExtension2Area	Ext2;					// existing version 2.0 extension area
	unsigned char		ExtensionVersion;		// = 0x30 for version 3.0
	unsigned long		ExtensionAttributes;	// bit flags
	unsigned short		ComponentCount;			// 1 for grey scale, 2 for greyscale w/alpha, 3 for RGB,
												//   4th assumed to be alpha,
												//   any extra are application specific
	unsigned short		ComponentBitDepth;		// 8 or 16, higher is permitted
	unsigned short		FixedPointFormat;		// 0x0008, 0x810E (Hub3 2.14), or 0x0010
	unsigned short		ComponentOrdering;		// ordering of the components in ARGB image; normally 0x3210
	unsigned short		AlphaChannelBitDepth;	// replaces the corresponding field in the image descriptor
	unsigned short		Colorspace;				// greyscale, RGB, YCbCr, YPbBr
	unsigned char		Reserved[3];			// must be set to all zeroes
	struct {
		unsigned char	Width;					// number of pixels on each line of macro block
		unsigned char	Height;					// number of lines in each macro block
		unsigned char	SampleCount;			// total number of luma and chroma samples in each macro block
		unsigned char	LumaSamples;			// low nibble is horizontal samples, high nibble is vertical samples
		unsigned char	ChromaSamples;			// low nibble is horizontal samples, high nibble is vertical samples
		struct {								// sequence of 2 bits: 0 = none, 1 = luma, 2 = Cb, 3 = Cr
			unsigned long Lo;
			unsigned long Hi;
		} SampleOrdering;
		unsigned long	CbOffset;				// offset of the Cb chroma data if not packed
		unsigned long	CrOffset;				// offset of the Cr chroma data if not packed
	} MacroInfo;
	struct {									// lowest component values in image data
		unsigned long	Blue;
		unsigned long	Green;
		unsigned long	Red;
	} BlackLevel;
	struct {									// highest component values in image data
		unsigned long	Blue;
		unsigned long	Green;
		unsigned long	Red;
	} WhiteLevel;
	struct {									// offset used for conversion from YCbCr
		unsigned long	Blue;
		unsigned long	Green;
		unsigned long	Red;
	} ComponentDelta;
	struct {									// separate gamma values per component
		TGAFraction		Blue;
		TGAFraction		Green;
		TGAFraction		Red;
		TGAFraction		Alpha;
	} ComponentGamma;
	unsigned long		ExternalAlphaOffset;	// offset to separate block of alpha data, relative to start of file
} TGAExtension3Area;


// The file footer is located at the end of the file.  This structure is 26
// bytes in length, and will be located 26 bytes from the end of the file.
// Not all TGA files will contain a file footer.  However, all TGA files that
// are created should have a footer written to the end of the file, even if
// no extension area or developer directory is located in the TGA file.
//
typedef struct tagTGAFileFooter {
	unsigned long	ExtensionAreaOffset;
	unsigned long	DeveloperDirectoryOffset;
	char			Signature[18];
} TGAFileFooter;


//////////////////////////////////////////////////////////////////////////////
//
//	The remainder of this file is present for purposes of backwards
//	compatibility with an alternate version of the TGA.h file in use by
//	some developers.
//
//////////////////////////////////////////////////////////////////////////////


/*
**	Attempt to accomodate machine dependencies
**	The parameter, MSDOS, is automatically defined by the Microsoft
**	compiler.
*/

//#if MSDOS
typedef signed char		SINT8;			/* 8-bit signed integer				*/
typedef unsigned char	UINT8;			/* 8-bit unsigned integer			*/
typedef signed short	SINT16;			/* 16-bit signed integer			*/
typedef unsigned short	UINT16;			/* 16-bit unsigned integer			*/
typedef long			SINT32;			/* 32-bit signed integer			*/
//typedef unsigned long	UINT32;			/* 32-bit unsigned integer			*/
//#endif

typedef struct picFile {
	unsigned char	idLength;	/* length of ID			*/
	unsigned char	mapType;	/* color map type		*/
	unsigned char	imageType;	/* image type			*/
	struct	mapHead{			/* map information		*/
		unsigned short	origin;	/* starting index of map */
		unsigned short	length;	/* length of map		*/
		unsigned char	width;	/* width of map in bits	*/
	} mapInfo;
	struct	imHead {
		unsigned short	x;		/* x-origin of image	*/
		unsigned short	y;		/* y-origin of image	*/
		unsigned short	dx;		/* dx-origin of image	*/
		unsigned short	dy;		/* dy-origin of image	*/
		unsigned char	depth;	/* bits per pixel		*/
		unsigned char	attrib;	/* attributes per pixel	*/
	} imageInfo;
} PICINFO;


/*
 * defined picture types
 */

#define	PIC_HD_SIZE	18

/* id info	*/

#define	MAXIDLEN	255


/* map types (currently defined map types)	*/

#define	NO_MAP			0	/* file has no color map	*/
#define	DEF_MAP			1	/* file has default color map, no compression	*/


/* image types	*/

#define	NO_IMAGE		0	/* file has no image	*/
#define	MAPPED_IMAGE	1	/* image color mapped, i.e. data are indices */
#define	RL_MAPPED_IMAGE	9	/* RL image color mapped		*/
#define	RGB_IMAGE		2	/* RGB data in image	*/
#define	RL_RGB_IMAGE	10	/* Run Length RGB data in image	*/
#define	BW_IMAGE		3	/* black & white image	*/
#define	RL_BW_IMAGE		11	/* Run Length black & white image	*/

/* interleaving types	*/

#define	INTLEV0	0		/* no interleave	*/
#define	INTLEV1	1		/* odd/even interleave	*/
#define	INTLEV2	2		/* 4-way interleave	*/

/* image types 32-128 are reserved for compression types	*/

#define	RDH_IMAGE		32	/* run-length, delta, huffman	*/
#define	RDHBLK_IMAGE	33	/* RDH, in block format		*/


/*
 * picture file error return codes
 */


#define	PIC_HD_ERR		-1	/* picture header error	*/

#define	ID_ERR			-2	/* id error */

#define	WID_ERR			-3	/* map width (size) error	*/
#define	MAP_ERR			-4	/* map error	*/

#define	IM_DEPTH_ERR	-5	/* error in image depth (pixel size)	*/
#define	IM_ERR			-6	/* error in image	*/
#define	IM_TYPE_ERR		-7	/* error in image type	*/



#define	EXT_SIZE_20			495

#define EXTENDED_SIG		"TRUEVISION-XFILE."
#define TV_FOOT_BYTES		26
#define CCYCLE_TAG			32768
#define SUBHDR_TAG			32769
#define WINMASK_DATA_TAG	32770

/*************************************/
#ifdef BMT_ALREADY_DEFINED_IN_DVRGRAPH
typedef struct _devDir
{
	UINT16	tagValue;
	unsigned long	tagOffset;
	unsigned long	tagSize;
} DevDir;
#endif // BMT_ALREADY_DEFINED_IN_DVRGRAPH
/*************************************/

typedef struct _TGAFileExt
{
	UINT16	extSize;			/* extension area size */
	char	author[41];			/* name of the author of the image */
	char	authorCom[4][81];	/* author's comments */
	UINT16	month;				/* date-time stamp */
	UINT16	day;
	UINT16	year;
	UINT16	hour;
	UINT16	minute;
	UINT16	second;
	char	jobID[41];			/* job identifier */
	UINT16	jobHours;			/* job elapsed time */
	UINT16	jobMinutes;
	UINT16	jobSeconds;
	char	softID[41];			/* software identifier/program name */
	UINT16	versionNum;			/* software version designation */
	UINT8	versionLet;
	unsigned long	keyColor;	/* key color value as A:R:G:B */
	UINT16	pixNumerator;		/* pixel aspect ratio */
	UINT16	pixDenominator;
	UINT16	gammaNumerator;		/* gamma correction factor */
	UINT16	gammaDenominator;
	unsigned long	colorCorrectOffset;	/* offset to color correction table */
	unsigned long	stampOffset;		/* offset to postage stamp data */
	unsigned long	scanLineOffset;		/* offset to scan line table */
	UINT8	alphaAttribute;				/* alpha attribute description */
} TGAFileExt;


typedef struct _TGAFooter
{
	unsigned long	extAreaOffset;	/* extension area offset */
	unsigned long	devDirOffset;	/* developer directory offset */
	char			signature[18];	/* signature string	*/
} TGAFooter;


// Restore whatever packing alignment was in use before this file was included.
//
// For some versions of GNU C, change to "#pragma pack()"
//
#pragma pack(pop, normalPacking)


#endif // APP_TgaFile_h
