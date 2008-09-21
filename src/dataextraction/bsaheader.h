#ifndef _BSA_HEADER_H_
#define _BSA_HEADER_H_

#include <iostream>
#include <boost/format.hpp>

class bsaheader {
  private:
    unsigned short height;
    unsigned short width;

  public:
    bsaheader();
    bsaheader(const unsigned short w, const unsigned short h);
    virtual ~bsaheader();

    virtual void set_height(unsigned short h);
    virtual void set_width(unsigned short w);

    virtual unsigned short get_height() const;
    virtual unsigned short get_width() const;

    virtual std::string to_string() const;
};

#endif /* _BSA_HEADER_H_ */
