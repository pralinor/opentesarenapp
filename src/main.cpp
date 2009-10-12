#include <iostream>
#include <fstream>
#include "dataextraction/bsaheadercfa.h"
#include "utils/subscriptableifstream.h"
#include "dataextraction/globalbsa.h"
#include "dataextraction/bsaindexentry.h"
#include "dataextraction/dataextractionmain.h"
#include "utils/utils.h"

void test_bsaheadercfa()
{
  bsaheadercfa bhcfa;

  std::cout << bhcfa.get_bits_per_pixel() << bhcfa.get_frame_count() << std::endl;
  std::cout << bhcfa << std::endl;

  subscriptableifstream s("/home/mano/teste.txt", std::ifstream::in);

  for (int i = 0; i < 10; i++) {
    if (!s.eof()) {
      char c = (char)(size_t)s[i];
      if (c != '\n') {
        std::cout << c << std::endl;
        s++;
      } else {
        break;
      }
    }
  }
}

void test_globalbsa()
{
  globalbsa gbsa("/home/mano/arena/GLOBAL.BSA");

  std::cout << "nr entries: " << gbsa.get_nr_entries() << std::endl;

  dataextractionmain dem();
}

void test_bsaindexentry()
{
  bsaindexentry bie("test", 1, 2, true);

  bie.set_name("XYZZY");
  std::cout << bie.to_string() << std::endl;
}

void test_strip_unprintable()
{
  char t[] = "123\adssdfsd\a";

  std::string s("6\a77\a");

  std::cout << utils::strip_unprintable(t) << std::endl;
  std::cout << *(utils::strip_unprintable(s)) << std::endl;
}

void test_dataextractionmain(const char* f) {
  dataextractionmain dem(f);
  std::cout << dem.to_string() << std::endl;
}

void test_rle() {
	unsigned short test[2];
	test[1] = 'a';
	test[2] = 'b';
	unsigned short rle_encoded[2];
	unsigned short rle_decoded[2];

	utils::encode_rle(test,rle_encoded, 2);
	std::cout << "rle encoded: " << rle_encoded[1] << rle_encoded[2] << std::endl;
	utils::decode_rle(rle_encoded, rle_decoded, 2);
	std::cout << "rle decoded: " << rle_decoded[1] << rle_decoded[2] << std::endl;
}

int main(int argc, char**argv) {

  //test_bsaindexentry();
  //test_strip_unprintable();
  if (argc >= 2) {
	  try {
              std::cout << "file: " << argv[1] << std::endl;
              //test_dataextractionmain(argv[1]);
              test_rle();
	  } catch (int e) {
              std::cout << "exception: " << e << std::endl;
	  }
  } else {
    std::cout << "no go! " << argc << std::endl;
  }

  return 0;
}
