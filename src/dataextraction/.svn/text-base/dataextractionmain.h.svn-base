#ifndef _DATAEXTRACTIONMAIN_H_
#define _DATAEXTRACTIONMAIN_H_

#include <boost/format.hpp>

#include "bsaindex.h"
#include "globalbsa.h"

class dataextractionmain {
  private:
    globalbsa gbsa;

  public:
    dataextractionmain();
    dataextractionmain(const char* gbsa_filename);
    dataextractionmain(const std::string& gbsa_filename);
    ~dataextractionmain();

    virtual globalbsa get_globalbsa() const;
    //virtual void set_globalbsa(const globalbsa& g);

    virtual std::string to_string() const;
};

#endif /* _DATAEXTRACTIONMAIN_H_*/
