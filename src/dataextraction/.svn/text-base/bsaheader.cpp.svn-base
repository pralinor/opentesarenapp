#include "bsaheader.h"

bsaheader::bsaheader():
  height(),
  width()
{
}

bsaheader::bsaheader(const unsigned short w, const unsigned short h):
  height(h),
  width(w)
{
}

bsaheader::~bsaheader()
{
}

void bsaheader::set_width(unsigned short w)
{
  width = w;
}

void bsaheader::set_height(unsigned short h)
{
  height = h;
}

unsigned short bsaheader::get_width() const
{
  return width;
}

unsigned short bsaheader::get_height() const
{
  return height;
}

std::string bsaheader::to_string() const
{
	boost::format fmt(
			"bsaheader: [width=%d|height=%d]");
	fmt % get_width() % get_height();
	return fmt.str();
}

