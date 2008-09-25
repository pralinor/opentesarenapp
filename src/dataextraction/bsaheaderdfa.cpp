#include "bsaheaderdfa.h"

bsaheaderdfa::bsaheaderdfa():
	bsaheader(),
	compressed_size(0),
	u2(0),
	u3(0),
	frame_count(0)
{
}

bsaheaderdfa::bsaheaderdfa(const unsigned short comp_sz, const unsigned short height,
    const unsigned short width, const unsigned short u2, const unsigned short u3,
    const unsigned char frame_c):
	bsaheader(width, height),
  compressed_size(comp_sz),
	u2(u2),
	u3(u3),
	frame_count(frame_c)
{
}

bsaheaderdfa::~bsaheaderdfa()
{
}

unsigned short bsaheaderdfa::get_compressed_size() const
{
	return compressed_size;
}

unsigned short bsaheaderdfa::get_u2() const
{
	return u2;
}

unsigned short bsaheaderdfa::get_u3() const
{
	return u3;
}

unsigned char bsaheaderdfa::get_frame_count() const
{
	return frame_count;
}

void bsaheaderdfa::set_compressed_size(const unsigned short comp_sz)
{
  compressed_size = comp_sz;
}

void bsaheaderdfa::set_u2(const unsigned short u2)
{
	this->u2 = u2;
}

void bsaheaderdfa::set_u3(const unsigned short u3)
{
	this->u3 = u3;
}

void bsaheaderdfa::set_frame_count(const unsigned char frame_c)
{
	this->frame_count = frame_count;
}

std::string bsaheaderdfa::to_string() const
{
  boost::format fmt("bsaheaderdfa: [width=%d|height=%d|compressed_size=%d|u2=%d|u3=%d|frame_count=%d]");

  fmt % get_width();
  fmt % get_height();
  fmt % compressed_size;
  fmt % u2;
  fmt % u3;
  fmt % frame_count;
  
  return fmt.str();
}

bsaheaderdfa& bsaheaderdfa::operator =(const bsaheaderdfa& other)
{
	if (this != &other) {
		set_width(other.get_width());
		set_height(other.get_height());
		set_compressed_size(other.compressed_size);
		set_u2(other.u2);
		set_u3(other.u3);
		set_frame_count(other.frame_count);
	}
	
	return *this;
}

std::ostream& operator << (std::ostream& os, const bsaheaderdfa& bhd)
{
  const std::string out = bhd.to_string();
  os << out << std::endl;
  return os;
}

std::istream& operator >> (std::istream& is, bsaheaderdfa& bhd)
{
  bhd.load(is);
  return is;
}

void bsaheaderdfa::load(std::istream& is)
{
  if (is.good()) {
    off_t current_position = is.tellg();
  }
}
