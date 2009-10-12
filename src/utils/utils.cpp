#include "utils.h"

long utils::size(std::istream &is) {
  long size = 0;
  off_t curr_pos = (off_t)0;

  if (is.good()) {
    curr_pos = is.tellg(); 

    is.seekg(0, std::ios::end);
    size = is.tellg();

    is.seekg(curr_pos, std::ios::beg);
  } else {
	  std::cout << " is bad" << std::endl;
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
		return static_cast<char>(hex - 'A' + 10);
	}

	return static_cast<char>(hex - '0');
}

unsigned int utils::decode_rle(unsigned short* raw_data, unsigned short* output, int stop_count)
{
	unsigned int i = 0, o = 0;

	while (o < stop_count) {
		unsigned short sample = raw_data[i++];

		// compressed packet
		if (sample & 0x80) {
			unsigned short value = raw_data[i++];
			unsigned int count = sample - 0x7F;

			for (int j = 0; j < count; j++){
				output[o++] = value;
			}
		} else {
			unsigned int count = sample + 1;

			for (int j = 0; j < count; j++){
				output[o++] = raw_data[i++];
			}
		}
	}

	return i;
}

unsigned int utils::encode_rle(unsigned short* input, unsigned short* raw_output, int stop_count= 0)
{
	short last_sample = -1;
	unsigned o = 0;
	unsigned int rle_counter = 0;

	for (unsigned int i = 0; i < stop_count; i++) {
		unsigned short sample = input[i];

		if (last_sample != -1) {
			if (last_sample == sample) {
				// increment counter
				rle_counter++;
			} else {
				// found something different
				raw_output[o++] = rle_counter;
				raw_output[o++] = last_sample;
				last_sample = sample;
				rle_counter = 1;
			}
		} else {
			last_sample = sample;
			rle_counter = 1;
		}
	}

	if (rle_counter > 0) {
		raw_output[o++] = rle_counter;
		raw_output[o++] = last_sample;
	}
}
