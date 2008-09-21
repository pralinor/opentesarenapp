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
};

#endif /* _UTILS_H_ */

