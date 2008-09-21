#include "bsaindexentry.h"

unsigned int bsaindexentry::NAME_LENGTH = 12;
unsigned int bsaindexentry::SIZE_LENGTH = 2;
unsigned int bsaindexentry::UNKNOWNS_LENGTH = 2;

bsaindexentry::bsaindexentry():
  name(),
  size(),
  offset(),
  has_override()
{
}

bsaindexentry::bsaindexentry(const std::string name, const unsigned long size, const unsigned long offset, const bool has_override):
  name(name),
  size(size),
  offset(offset),
  has_override(has_override)
{
}

bsaindexentry::bsaindexentry(const char* name, const unsigned long size, const unsigned long offset, const bool has_override):
  name(name),
  size(size),
  offset(offset),
  has_override(has_override)
{
}

bsaindexentry::~bsaindexentry()
{
}

void bsaindexentry::set_name(const std::string name)
{
  this->name = name;
}

void bsaindexentry::set_name(const char* name)
{
  this->name = name;
}

void bsaindexentry::set_size(const unsigned long size)
{
  this->size = size;
}

void bsaindexentry::set_offset(const unsigned long offset)
{
  this->offset = offset;
}

void bsaindexentry::set_has_override(const bool has_override)
{
  this->has_override = has_override;
}

std::string bsaindexentry::get_name() const
{
  return name;
}

unsigned long bsaindexentry::get_size() const
{
  return size;
}

unsigned long bsaindexentry::get_offset() const
{
  return offset;
}

bool bsaindexentry::get_has_override() const
{
  return has_override;
}

std::string bsaindexentry::to_string() const
{
	boost::format fmt("bsaindexentry: [name=%s|size=%d|offset=%d|has_override=%d]");
	fmt % get_name() % get_size() % get_offset() % get_has_override();
	return fmt.str();
}

void bsaindexentry::read(std::istream& file)
{
  //char n[bsaindexentry::NAME_LENGTH];

  char *n = new char[bsaindexentry::NAME_LENGTH];
  try {
    has_override = false;

    file.read(n, bsaindexentry::NAME_LENGTH);

    char *dot = strchr(n, '.');
    int where;
    if (dot == NULL)
      where = strlen(n);
    else
      where = dot - n + 4;

    name.insert(0, n, where);

    file.read(n, bsaindexentry::UNKNOWNS_LENGTH);

    file.read(reinterpret_cast<char *>(&size), bsaindexentry::SIZE_LENGTH);

    file.read(n, bsaindexentry::UNKNOWNS_LENGTH);
  } catch (...) {
    delete [] n;
    throw;
  }
  delete [] n;
}

void bsaindexentry::load(std::istream& file)
{
  read(file);
}
