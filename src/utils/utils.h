#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <fstream>

#include <cctype>
#include <cstring>

class utils {
  public:
    static long size(std::istream &is);
    static std::string* strip_unprintable(const std::string s);
    static char* strip_unprintable(const char* s);
    static int hex_to_int(char hex);
    static unsigned int decode_rle(unsigned short* raw_data, unsigned short* output, int stop_count);
    static unsigned int encode_rle(unsigned short* input, unsigned short* raw_output, int stop_count);
};

#endif /* _UTILS_H_ */

