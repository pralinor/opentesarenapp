#include "bsafontelement.h"

bsafontelement::bsafontelement():
  width(),
  height(),
  lines()
{
}

bsafontelement::bsafontelement(const unsigned short w, const unsigned short h):
  width(w),
  height(h),
  lines()
{
}

bsafontelement::~bsafontelement()
{
}

unsigned short bsafontelement::get_height() const {
  return height;
}

void bsafontelement::set_height(const unsigned short h) {
  height = h;
}

unsigned short bsafontelement::get_width() const {
  return width;
}

void bsafontelement::set_width(const unsigned short w) {
  width = w;
}

std::vector<unsigned short>* bsafontelement::get_lines() const {
  std::vector<unsigned short>* copy = new std::vector<unsigned short>(lines);

  return copy;
}

void bsafontelement::add_line(const unsigned short line) {
  lines.push_back(line);
}

std::string bsafontelement::to_string() const {
  /* TODO: add lines to the output */
  boost::format fmt("bsafontelement: [width=%d|height=%d");
  fmt % get_width() % get_height();

  return fmt.str();
}
