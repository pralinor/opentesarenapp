#include "globalbsa.h"

const std::ios::openmode globalbsa::OPEN_MODE = std::ios::in | std::ios::binary;

const char* globalbsa::CODE_TABLE [16] = \
        {"AB89EFCD23016745", // [address % 16] == 0
        "CDEF89AB45670123",
        "0123456789ABCDEF",
        "0123456789ABCDEF",
        "DCFE98BA54761032",  // [address % 16] == 4
        "EFCDAB8967452301",
        "EFCDAB8967452301",
        "0123456789ABCDEF",
        "23016745AB89EFCD",  // [address % 16] == 8
        "45670123CDEF89AB",
        "89ABCDEF01234567",
        "89ABCDEF01234567",
        "54761032DCFE98BA",  // [address % 16] == 12
        "67452301EFCDAB89",
        "67452301EFCDAB89",
        "89ABCDEF01234567"};

globalbsa::globalbsa():
  filename(),
  file_size(),
  global_bsa_stream(),
  nr_entries(),
  index()
{
}

globalbsa::globalbsa(const char *filename):
  filename(filename),
  file_size(),
  global_bsa_stream(filename, globalbsa::OPEN_MODE),
  nr_entries(),
  index()
{
  init();
}

globalbsa::globalbsa(const std::string filename):
  filename(filename),
  file_size(),
  global_bsa_stream(filename, globalbsa::OPEN_MODE),
  nr_entries(),
  index()
{
  init();
}

globalbsa::globalbsa(const globalbsa& other):
  filename(other.get_filename()),
  file_size(),
  global_bsa_stream(other.get_filename(), globalbsa::OPEN_MODE),
  nr_entries(),
  index()
{
  init();
}

globalbsa::~globalbsa()
{
}

void globalbsa::init()
{
  file_size = utils::size(global_bsa_stream);

  read_number_of_index_entries();
  read_index();
}

/*
 * read the first 16 bits (2 bytes), the number of 
 * index entries on the file
 */
void globalbsa::read_number_of_index_entries()
{
  global_bsa_stream.read(reinterpret_cast<char *>(&nr_entries), 2);
}

long globalbsa::get_nr_entries() const
{
  return nr_entries;
}

void globalbsa::set_nr_entries(const long nr)
{
  nr_entries = nr;
}

std::string globalbsa::get_filename() const
{
  return filename;
}

void globalbsa::set_filename(const std::string fname)
{
  filename = fname;
}

void globalbsa::set_filename(const char* fname)
{
  filename = fname;
}

off_t globalbsa::get_file_size() const
{
  return file_size;
}

void globalbsa::set_file_size(const off_t file_size)
{
  this->file_size = file_size;
}

void globalbsa::read_index()
{
  index.set_entry_count(nr_entries);
  index.set_offset(0);
  index.set_file_size(static_cast<unsigned long>(file_size));
  index.load(global_bsa_stream);
}

bsaindex globalbsa::get_index() const
{
  return index;
}

void globalbsa::set_index(const bsaindex& idx)
{
  index = idx;
}

std::string globalbsa::to_string() const
{
	boost::format fmt(
			"globalbsa: [filename=%s|file_size=%d|nr_entries=%d|index=%s]");
	fmt % get_filename() % static_cast<long>(get_file_size()) % get_nr_entries() % index.to_string();
	return fmt.str();
}

void globalbsa::make_inverse_code_table()
{
  for (int y = 0; y < INVERSE_CODE_TABLE_SIZE; y++) {
    for (int x = 0; x < INVERSE_CODE_TABLE_SIZE; x++) {
      int inv = utils::hex_to_int(CODE_TABLE[y][x]);

      inverse_code_table[y][inv] = x;
    }
  }
}
