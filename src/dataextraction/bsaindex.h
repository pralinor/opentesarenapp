#ifndef _BSAINDEX_H_
#define _BSAINDEX_H_

#include <iostream>
#include <boost/format.hpp>

#include <map>
#include <list>

#include "bsaindexentry.h"

class bsaindex {
  private:
    const static unsigned short SIZE;

    enum ENTRY_TYPES
    {
      VOCS,
      CFAS,
      DFAS,
      CIFS
    };

    unsigned long entry_count;
    unsigned long offset;
    std::map<std::string, bsaindexentry> entries;
    unsigned long file_size;

  public:
    bsaindex();
    bsaindex(const unsigned int entry_count, const unsigned long offset, const unsigned long file_size);
    ~bsaindex();


    virtual void set_entry_count(const unsigned long entry_count);
    virtual void set_offset(const unsigned long offset);
    virtual void set_entries(const std::map<std::string, bsaindexentry> entries);
    virtual void set_file_size(const unsigned long file_size);

    virtual unsigned int get_entry_count() const;
    virtual unsigned long get_offset() const;
    virtual std::map<std::string, bsaindexentry> get_entries() const;
    virtual unsigned long get_file_size() const;
    virtual std::string to_string() const;

    virtual std::map<std::string, bsaindexentry>::iterator begin();
    virtual std::map<std::string, bsaindexentry>::const_iterator begin() const;
    virtual std::map<std::string, bsaindexentry>::iterator end();
    virtual std::map<std::string, bsaindexentry>::const_iterator end() const;

    virtual bsaindexentry& operator[] (const std::string& key);
    
    virtual std::list<bsaindexentry>* get_entries(bsaindex::ENTRY_TYPES entry_type) const;
    virtual std::list<bsaindexentry>* get_voc_entries() const;
    virtual std::list<bsaindexentry>* get_cfa_entries() const;
    virtual std::list<bsaindexentry>* get_dfa_entries() const;
    virtual std::list<bsaindexentry>* get_cif_entries() const;

    virtual void load(std::istream& is);

    virtual void operator= (const bsaindex& other);
};

#endif /* _BSAINDEX_H_ */
