#ifndef _BSADFA_H_
#define _BSADFA_H_

/*
 * DFAs are a form of animation, but simpler than CFA.  Whereas CFAs have a
 * separate image per frame, DFAs store a single base frame with animation
 * changing a portion of it (e.g., the bartender sprite only animates the
 * hand).  However, some DFAs completely replace the image contents every
 * frame (the crystal ball and some guard sprites completely clear the image
 * for some frames).
 *
 * The base frame is encoded using only RLE.  This is immediately followed
 * by a block of replacement values encoded as a byte offset + new color.
 */

#include "bsaheader.h"

class bsadfa {
};

#endif /* _BSADFA_H_ */
