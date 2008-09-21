#ifndef _BSACFA_H_
#define _BSACFA_H_

/*
 *   Decodes a creature animation.  This will take the form of several frames
 *   packed into a single buffer as a slideshow sequence.  The frames are
 *   packed, without any extra data between them.
 * 
 *   This function takes an array of frame buffers.  The images encoded in the
 *   CFA are decoded, then copied one at a time into the array of frame buffers.
 * 
 *   The CFA format is subjected to several levels of encoding.
 *    1) First off, the base palettized image is palettized again.  A look-up
 *       table is stored at the beginning of the animation, containing only
 *       those color indices that are used by the animation.
 *    2) Each re-palettized index value is packed (muxed) into as few bits as
 *       possible (e.g., if there are only 7 distinct colors, each encoded
 *       value is stored in 3 bits).  The bit data is packed up to 8-bit
 *       alignment, so eight 3-bit values will be packed into 24 bits, filling
 *       3 bytes.  If the image width is not a multiple of this value, extra
 *       data is stored after the end of the line to pad it to the appropriate
 *       multiple of bytes.  This is why the CFA header distinguishes between
 *       the image's compressed and uncompressed widths.
 *    3) The resulting image data is subjected to run-length encoding.  Since
 *       all CFAs contain a substantial amount of transparent black, this gives
 *       most of the compression.
 */

#include "bsaheader.h"

class bsacfa {
};

#endif /* _BSACFA_H_ */
