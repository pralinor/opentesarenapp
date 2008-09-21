#include "bsaheadercfa.h"

bsaheadercfa::bsaheadercfa():
	bsaheader(),
	width_compressed(0),
	u2(0),
	u3(0),
	bits_per_pixel(0),
	frame_count(0),
	header_size(0)
{
}

bsaheadercfa::bsaheadercfa(const unsigned short width_unc, const unsigned short height,
    const unsigned short width_c, const unsigned short u2, const unsigned short u3,
    const unsigned char bpp, const unsigned char frame_c, const unsigned short header_s):
	bsaheader(width_unc, height),
	width_compressed(width_c),
	u2(u2),
	u3(u3),
	bits_per_pixel(bpp),
	frame_count(frame_c),
	header_size(header_s)
{
}

bsaheadercfa::~bsaheadercfa()
{
}

unsigned short bsaheadercfa::get_width_uncompressed() const
{
	return this->get_width();
}

unsigned short bsaheadercfa::get_width_compressed() const
{
	return this->width_compressed;
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
	return this->bits_per_pixel;
}

unsigned char bsaheadercfa::get_frame_count() const
{
	return this->frame_count;
}

unsigned short bsaheadercfa::get_header_size() const
{
	return this->header_size;
}

void bsaheadercfa::set_width_uncompressed(const unsigned short w)
{
  this->set_width(w);
}

void bsaheadercfa::set_width_compressed(const unsigned short w)
{
	this->width_compressed = w;
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
	this->bits_per_pixel = bpp;
}

void bsaheadercfa::set_frame_count(const unsigned char frame_count)
{
	this->frame_count = frame_count;
}

void bsaheadercfa::set_header_size(const unsigned short header_size)
{
	this->header_size = header_size;
}

std::string bsaheadercfa::to_string() const
{
  char fmt_str[] = "bsaheadercfa: [width_unc=%d|height=%d|width_c=%d|u2=%d|u3=%d|bpp=%d|frame_count=%d|header_size=%d]";
  boost::format fmt(fmt_str);

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
  bhc.load(is);
  return is;
}

void bsaheadercfa::load(std::istream& is)
{
  if (is.good()) {
    off_t current_position = is.tellg();
  }
}
