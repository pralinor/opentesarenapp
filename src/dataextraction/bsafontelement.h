#ifndef _BSA_FONTELEMENT_H_
#define _BSA_FONTELEMENT_H_

#include <iostream>
#include <vector>
#include <boost/format.hpp>

class bsafontelement {
  private:
    unsigned short width;
    unsigned short height;
    // TODO: check if this is an int
    std::vector<unsigned short> lines;

  public:
    bsafontelement();
    bsafontelement(const unsigned short w, const unsigned short h);
    virtual ~bsafontelement();

    virtual unsigned short get_height() const;
    virtual void set_height(const unsigned short h);

    virtual unsigned short get_width() const;
    virtual void set_width(const unsigned short w);

    virtual void add_line(const unsigned short line);

    virtual std::vector<unsigned short>* get_lines() const;
    virtual void set_lines(const std::vector<unsigned short> l);
    
    virtual std::string to_string() const;
};

#endif /* _BSA_FONTELEMENT_H_ */
