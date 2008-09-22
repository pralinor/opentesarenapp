#ifndef _BSA_FONTELEMENT_H_
#define _BSA_FONTELEMENT_H_

#include <iostream>
#include <vector>
#include <boost/format.hpp>

class bsafontelement {
  private:
    short height;
    short width;
    // TODO: check if this is an int
    std::vector<int> lines;

  public:
    bsafontelement();
    bsafontelement(const unsigned short w, const unsigned short h);
    virtual ~bsafontelement();

    virtual std::string to_string() const;
};

#endif /* _BSA_FONTELEMENT_H_ */
