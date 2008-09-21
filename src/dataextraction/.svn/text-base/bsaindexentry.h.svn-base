#ifndef _BSAINDEXENTRY_H_
#define _BSAINDEXENTRY_H_

#include <iostream>
#include <boost/format.hpp>

#include "../utils/utils.h"

class bsaindexentry {
  private:
    std::string name;
    unsigned long size;
    unsigned long offset;
    bool has_override;

    /* static stuff */
    static unsigned int NAME_LENGTH;
    static unsigned int SIZE_LENGTH;
    static unsigned int UNKNOWNS_LENGTH;

  public:
    bsaindexentry();
    bsaindexentry(const std::string name, const unsigned long size, const unsigned long offset, const bool has_override);
    bsaindexentry(const char *name, const unsigned long size, const unsigned long offset, bool has_override);
    ~bsaindexentry();

    virtual void set_name(const std::string name);
    virtual void set_name(const char* name);
    virtual void set_size(const unsigned long size);
    virtual void set_offset(const unsigned long offset);
    virtual void set_has_override(const bool has_override);

    virtual std::string get_name() const;
    virtual unsigned long get_size() const;
    virtual unsigned long get_offset() const;
    virtual bool get_has_override() const;

    virtual std::string to_string() const;

    virtual void read(std::istream& file);
    virtual void load(std::istream& file);
};

#endif /* _BSAINDEXENTRY_H_ */
