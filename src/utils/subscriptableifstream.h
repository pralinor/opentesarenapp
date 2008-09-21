#ifndef _SUBSCRIPTABLEIFSTREAM_H_
#define _SUBSCRIPTABLEIFSTREAM_H_

#include <iostream>
#include <istream>
#include <fstream>

class subscriptableifstream: public std::ifstream {
  private:
    std::string filename;

  public:
    subscriptableifstream();
    subscriptableifstream(const char* filename, const std::ios::openmode mode);
    subscriptableifstream(const std::string filename, const std::ios::openmode mode);
    ~subscriptableifstream();
    
    virtual std::string get_filename() const;
    virtual void set_filename(const std::string& fname);
    virtual void set_filename(const char* fname);

    virtual void* operator[] (const int pos);
    virtual void operator+= (const int i);
    virtual void operator++ (const int dummy);
};

#endif /* _SUBSCRIPTABLEIFSTREAM_H_ */
