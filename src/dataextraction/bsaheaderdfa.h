#ifndef _BSA_HEADER_DFA_H_
#define _BSA_HEADER_DFA_H_

#include <iostream>

#include "bsaheader.h"

#include <boost/format.hpp>

class bsaheaderdfa: public bsaheader {
  private:
    unsigned short compressed_size;
    unsigned short u2;
    unsigned short u3;
    unsigned char frame_count;

  public:
    bsaheaderdfa();
    bsaheaderdfa(const unsigned short comp_sz, const unsigned short height,
        const unsigned short width, const unsigned short u2, const unsigned short u3,
        const unsigned char frame_c);
    virtual ~bsaheaderdfa();

    virtual unsigned short get_compressed_size() const;
	  virtual unsigned short get_u2() const;
	  virtual unsigned short get_u3() const;
	  virtual unsigned char get_frame_count() const;

	  virtual void set_compressed_size(const unsigned short comp_sz);
	  virtual void set_u2(const unsigned short u2);
	  virtual void set_u3(const unsigned short u3);
	  virtual void set_frame_count(const unsigned char frame_c);
	
	  virtual std::string to_string() const;

    bsaheaderdfa& operator = (const bsaheaderdfa& other);
	  friend std::ostream& operator << (std::ostream& ofs,
        const bsaheaderdfa& bhd);
	  friend std::istream& operator >> (std::istream& ifs,
        bsaheaderdfa& bhd);

    virtual void load(std::istream& is);
};

#endif /* _BSA_HEADER_DFA_H_ */
