#include "bsacfa.h"

bsacfa::bsacfa():
	header(),
	offset(),
	size(),
	filename(),
	pitch(),
	frames()
{
}

bsacfa::~bsacfa()
{
}

std::string bsacfa::get_filename() const
{
    return filename;
}

bsaheadercfa bsacfa::get_header() const
{
    return header;
}

long bsacfa::get_offset() const
{
    return offset;
}

int bsacfa::get_size() const
{
    return size;
}

void bsacfa::set_filename(std::string filename)
{
    this->filename = filename;
}

void bsacfa::set_header(bsaheadercfa header)
{
    this->header = header;
}

void bsacfa::set_offset(long  offset)
{
    this->offset = offset;
}

void bsacfa::set_size(int size)
{
    this->size = size;
}

void bsacfa::load(std::istream& is)
{
	// load the header
	header.load(is, offset);

	pitch = header.get_width_uncompressed();

	unsigned short frame_c = header.get_frame_count();
	unsigned short width_unc = header.get_width_uncompressed();
	unsigned short h = header.get_height();

	frames = new unsigned int* [frame_c];

	for (int i = 0; i < frame_c; i++)
		frames[i] = new unsigned int[width_unc * h];

	// Get the address where the look-up conversion table is located. This is
    // used to map the packed color values back to useful palette values.
    unsigned short remap_offset = offset + 0x4c;

    unsigned long old_offset = is.tellg();

    is.seekg(remap_offset + header.get_header_size(), std::ios::beg);

    // Create a line buffer on the stack.  Generously over-allocate it, since a
	// few extra bytes may be required while demuxing.
    unsigned int* encoded_data = new unsigned int[width_unc + 16];

    memset(encoded_data, 0, width_unc + 16);

	// Demuxing will return 2-8 index values per pass.  We'll store them
	// here while translating them into actual color values.
    unsigned int translate[8];

	// Allocate a worst-case buffer to hold the de-RLE'd data, since we need
	// to deal with padding for demux alignment.
	unsigned int* decomp = new unsigned int [width_unc * h * frame_c * sizeof(frame_c) + width_unc * 16];

	// The first step is to uncompress the run-length encoded data.  Once
	// this is done, the buffer will contain the bit-packed data, and will
	// also have a predictable size, so we can compute the start of each
	// packed line using pHeader->WidthCompressed.
	//DecodeRLE(pStart, pDecomp, pHeader->WidthCompressed * height * DWORD(pHeader->FrameCount));

}

void bsacfa::operator= (const bsacfa& other)
{
  if (this != &other) {
	  set_filename(other.get_filename());
	  set_header(other.get_header());
	  set_offset(other.get_offset());
	  set_size(other.get_size());
  }
}
