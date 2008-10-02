#include "bsapalette.h"

unsigned short bsapalette::ENTRY_NUMBER = 3;

unsigned short bsapalette::PALETTE_SIZE = 256;

bsapalette::bsapalette():
  filename(),
  palette()
{
  init_palette();
}

bsapalette::bsapalette(const std::string fname):
  filename(fname),
  palette()
{
  init_palette();
}

bsapalette::bsapalette(const char* fname):
  filename(fname),
  palette()
{
  init_palette();
}

bsapalette::~bsapalette()
{
}

void bsapalette::init_palette()
{
  for (int i = 0; i < PALETTE_SIZE; i++)
    palette[i] = 0;
}

std::string bsapalette::get_filename() const
{
  return filename;
}

std::vector<unsigned short> bsapalette::get_palette() const
{
  return palette;
}

void bsapalette::set_filename(const std::string fname)
{
  filename = fname;
}

void bsapalette::set_filename(const char* fname)
{
  filename = fname;
}

void bsapalette::set_palette(const std::vector<unsigned short> pal)
{
  palette = pal;
}

std::string bsapalette::to_string() const
{
  boost::format fmt("bsapalette: [filename=%s]");

  fmt % get_filename();

  return fmt.str();
}

