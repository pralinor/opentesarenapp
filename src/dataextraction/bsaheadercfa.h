#ifndef _BSAHEADERCFA_H_
#define _BSAHEADERCFA_H_

#include <iostream>

#include "bsaheader.h"

#include <boost/format.hpp>

class bsaheadercfa: public bsaheader {
  private:
    unsigned short width_uncompressed;
    unsigned short width_compressed;
    unsigned short u2;
    unsigned short u3;
    unsigned char bits_per_pixel;
    unsigned char frame_count;
    unsigned short header_size;

  public:
    bsaheadercfa();
    bsaheadercfa(const unsigned short w_unc, const unsigned short height,
        const unsigned short w_comp, const unsigned short u2, const unsigned short u3,
        const unsigned char bpp, const unsigned char frame_c, const unsigned short header_sz);
    virtual ~bsaheadercfa();

    virtual unsigned short get_width_uncompressed() const;
	  virtual unsigned short get_width_compressed() const;
	  virtual unsigned short get_u2() const;
	  virtual unsigned short get_u3() const;
	  virtual unsigned char get_bits_per_pixel() const;
	  virtual unsigned char get_frame_count() const;
	  virtual unsigned short get_header_size() const;

    virtual void set_width_uncompressed(const unsigned short width_unc);
	  virtual void set_width_compressed(const unsigned short width_c);
	  virtual void set_u2(const unsigned short u2);
	  virtual void set_u3(const unsigned short u3);
	  virtual void set_bits_per_pixel(const unsigned char bpp);
	  virtual void set_frame_count(const unsigned char frame_c);
	  virtual void set_header_size(const unsigned short header_s);
	
	  virtual std::string to_string() const;

    bsaheadercfa& operator = (const bsaheadercfa& other);
	  friend std::ostream& operator << (std::ostream& ofs,
        const bsaheadercfa& bhc);
	  friend std::istream& operator >> (std::istream& ifs,
        bsaheadercfa& bhc);

    virtual void load(std::istream& is);
};

#endif /* _BSAHEADERCFA_H_ */
