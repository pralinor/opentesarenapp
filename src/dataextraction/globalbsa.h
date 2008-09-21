#ifndef _GLOBALBSA_H_
#define _GLOBALBSA_H_

#include <iostream>
#include <boost/format.hpp>

#include "../utils/utils.h"
#include "../utils/subscriptableifstream.h"
#include "bsaindex.h"

class globalbsa {
  private:
    /* static stuff */
    const static char *CODE_TABLE[];
    const static short NAME_SIZE = 16;
    const static short SIZE_LENGTH = 2;
    const static short UNKNOWNS_LENGTH = 2;
    const static short INVERSE_CODE_TABLE_SIZE = 16;
    const static std::ios::openmode OPEN_MODE;

    std::string filename;
    off_t file_size;
    subscriptableifstream global_bsa_stream;
    //std::ifstream ifs;
    unsigned long nr_entries;
    bsaindex index;
    int inverse_code_table [INVERSE_CODE_TABLE_SIZE][INVERSE_CODE_TABLE_SIZE];

    /* private functions */
    void init();
    void read_number_of_index_entries();
    void read_index();
  public:
    globalbsa();
    globalbsa(const char *filename);
    globalbsa(const std::string filename);
    globalbsa(const globalbsa& other);
    ~globalbsa();

    virtual long get_nr_entries() const;
    virtual void set_nr_entries(const long nr);

    virtual std::string get_filename() const;
    virtual void set_filename(const std::string fname);
    virtual void set_filename(const char* fname);

    virtual off_t get_file_size() const;
    virtual void set_file_size(const off_t file_size);

    virtual bsaindex get_index() const;
    virtual void set_index(const bsaindex& idx);

    virtual std::string to_string() const;

    virtual void make_inverse_code_table();
};

#endif /* _GLOBALBSA_H_ */
