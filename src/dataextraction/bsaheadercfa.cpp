#include "bsaheadercfa.h"

bsaheadercfa::bsaheadercfa():
	bsaheader(),
	width_compressed(0),
	u2(0),
	u3(0),
	bits_per_pixel(0),
	frame_count(0)
{
}

bsaheadercfa::bsaheadercfa(const unsigned short width_unc, const unsigned short height,
    const unsigned short width_c, const unsigned short u2, const unsigned short u3,
    const unsigned char bpp, const unsigned char frame_c):
	bsaheader(width_unc, height),
	width_compressed(width_c),
	u2(u2),
	u3(u3),
	bits_per_pixel(bpp),
	frame_count(frame_c)
{
}

bsaheadercfa::~bsaheadercfa()
{
}

unsigned short bsaheadercfa::get_width_uncompressed() const
{
	return get_width();
}

unsigned short bsaheadercfa::get_width_compressed() const
{
	return width_compressed;
}

unsigned short bsaheadercfa::get_u2() const
{
	return u2;
}

unsigned short bsaheadercfa::get_u3() const
{
	return u3;
}

unsigned char bsaheadercfa::get_bits_per_pixel() const
{
	return bits_per_pixel;
}

unsigned char bsaheadercfa::get_frame_count() const
{
	return frame_count;
}

unsigned short bsaheadercfa::get_header_size() const
{
	return CFA_HEADER_SIZE_IN_BYTES;
}

void bsaheadercfa::set_width_uncompressed(const unsigned short w)
{
  set_width(w);
}

void bsaheadercfa::set_width_compressed(const unsigned short w)
{
	width_compressed = w;
}

void bsaheadercfa::set_u2(const unsigned short u2)
{
	this->u2 = u2;
}

void bsaheadercfa::set_u3(const unsigned short u3)
{
	this->u3 = u3;
}

void bsaheadercfa::set_bits_per_pixel(const unsigned char bpp)
{
	bits_per_pixel = bpp;
}

void bsaheadercfa::set_frame_count(const unsigned char frame_c)
{
	frame_count = frame_c;
}

void bsaheadercfa::set_header_size(const unsigned short header_s)
{
  header_size = header_s;
}

std::string bsaheadercfa::to_string() const
{
  boost::format fmt("bsaheadercfa: [width_unc=%d|height=%d|width_c=%d|u2=%d|u3=%d|bpp=%d|frame_count=%d|header_size=%d]");

  fmt % get_width();
  fmt % get_height();
  fmt % width_compressed;
  fmt % u2;
  fmt % u3;
  fmt % static_cast<int>(bits_per_pixel);
  fmt % static_cast<int>(frame_count);
  fmt % header_size;
  
  return fmt.str();
}

bsaheadercfa& bsaheadercfa::operator =(const bsaheadercfa& other)
{
	if (this != &other) {
		set_width_uncompressed(other.get_width());
		set_height(other.get_height());
		set_width_compressed(other.width_compressed);
		set_u2(other.u2);
		set_u3(other.u3);
		set_bits_per_pixel(other.bits_per_pixel);
		set_frame_count(other.frame_count);
    set_header_size(other.header_size);
	}
	
	return *this;
}

std::ostream& operator << (std::ostream& os, const bsaheadercfa& bhc)
{
  const std::string out = bhc.to_string();
  os << out << std::endl;
  return os;
}

std::istream& operator >> (std::istream& is, bsaheadercfa& bhc)
{
  off_t off = is.tellg();
  bhc.load(is, off);
  return is;
}

void bsaheadercfa::load(std::istream& is, const unsigned int offset)
{
  if (is.good()) {
    off_t current_position = is.tellg();

    is.seekg(offset, std::ios::beg);

    char *buf = new char[CFA_HEADER_SIZE_IN_BYTES];

    is.read(buf, CFA_HEADER_SIZE_IN_BYTES);

    HeaderCFA *h = reinterpret_cast<HeaderCFA *>(buf);
  }
}
