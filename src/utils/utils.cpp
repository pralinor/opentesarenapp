#include "utils.h"

long utils::size(std::istream &is) {
  long size = 0;
  off_t curr_pos = (off_t)0;

  if (is.good()) {
    curr_pos = is.tellg(); 

    is.seekg(0, std::ios::end);
    size = is.tellg();

    is.seekg(curr_pos, std::ios::beg);
  }

  return size;
}

std::string* utils::strip_unprintable(const std::string s)
{
  std::string* ret = new std::string(s);

  std::string::iterator it;

  for (it = ret->begin(); it < ret->end(); it++) {
    char c = *it;

    if (!isprint(c))
      ret->erase(it);
  }

  return ret;
}

char* utils::strip_unprintable(const char* s)
{
  int len = strlen(s);
  if (len > 0) {
    char *ret = new char[len];
    char *tmp, *tmp_ret;

    tmp_ret = ret;
    tmp = const_cast<char *>(s);
    for (; *tmp != '\0'; tmp++) {
      if (isprint(*tmp))
        *tmp_ret++ = *tmp;
    }

    return ret;
  } else {
    return NULL;
  }
}

int utils::hex_to_int(char hex) {
	if ((hex >= 'A') && (hex <= 'F')) {
		return char(hex - 'A' + 10);
	}

	return char(hex - '0');
}
