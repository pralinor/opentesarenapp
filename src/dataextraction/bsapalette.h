#ifndef _BSAPALETTE_H_
#define _BSAPALETTE_H_

#include <string>
#include <vector>

#include <boost/format.hpp>

class bsapalette {
  private:
    static unsigned short ENTRY_NUMBER;
    static unsigned short PALETTE_SIZE;

    std::string filename;
    std::vector<unsigned short> palette;

    void init_palette();
  public:
    bsapalette();
    bsapalette(const std::string fname);
    bsapalette(const char* fname);
    virtual ~bsapalette();

    virtual std::string get_filename() const;
    virtual std::vector<unsigned short> get_palette() const;
    virtual void set_filename(const std::string fname);
    virtual void set_filename(const char* fname);
    virtual void set_palette(const std::vector<unsigned short> pal);
    virtual std::string to_string() const;
};

#endif /* _BSAPALETTE_H_ */
