#include "subscriptableifstream.h"

subscriptableifstream::subscriptableifstream():
  filename()
{
}


subscriptableifstream::subscriptableifstream(const char* filename, const std::ios::openmode mode):
  std::ifstream(filename, mode),
  filename(filename)
{
}

subscriptableifstream::subscriptableifstream(const std::string filename, const std::ios::openmode mode):
  std::ifstream(filename.c_str(), mode),
  filename(filename)
{
}

subscriptableifstream::~subscriptableifstream()
{
}

std::string subscriptableifstream::get_filename() const
{
  return filename;
}

void subscriptableifstream::set_filename(const std::string& fname)
{
  filename = fname;
}

void subscriptableifstream::set_filename(const char* fname)
{
  filename = fname;
}

void* subscriptableifstream::operator[] (const int pos)
{
  if (good()) {
    /*
	void* temp = (void*) get();
    unget();
    */
	void *temp = (void*)peek();
    return temp;
  } else {
    return NULL;
  }
}

void subscriptableifstream::operator+= (const int i)
{
  if (!eof())
    seekg(i, std::ios::cur);
}

void subscriptableifstream::operator++ (const int dummy)
{
  *this += 1;
}

void subscriptableifstream::start(void) {
	seekg(0, std::ios::beg);
}

void subscriptableifstream::end(void) {
	seekg(0, std::ios::end);
}
