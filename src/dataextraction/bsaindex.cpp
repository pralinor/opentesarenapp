#include "bsaindex.h"

const unsigned short bsaindex::SIZE = 18;

bsaindex::bsaindex():
  entry_count(),
  offset(),
  entries(),
  file_size()
{
}

bsaindex::bsaindex(const unsigned int entry_count, const unsigned long offset, const unsigned long file_size):
  entry_count(entry_count),
  offset(offset),
  entries(),
  file_size(file_size)
{
}

bsaindex::~bsaindex()
{
}

void bsaindex::set_entry_count(const unsigned long entry_count)
{
  this->entry_count = entry_count;
}

void bsaindex::set_offset(const unsigned long offset)
{
  this->offset = offset;
}

void bsaindex::set_entries(const std::map<std::string, bsaindexentry> entries)
{
  this->entries = entries;
}

void bsaindex::set_file_size(const unsigned long file_size)
{
  this->file_size = file_size;
}

unsigned int bsaindex::get_entry_count() const
{
  return entry_count;
}

unsigned long bsaindex::get_offset() const
{
  return offset;
}

std::map<std::string, bsaindexentry> bsaindex::get_entries() const
{
  return this->entries;
}

unsigned long bsaindex::get_file_size() const
{
  return file_size;
}

std::string bsaindex::to_string() const
{
 	boost::format fmt("bsaindex: [entry_count=%d|offset=%d|entries=%s|file_size=%d]");
 	std::string temp("<");

 	if (entries.size() > 0) {
 		std::map<std::string, bsaindexentry>::const_iterator it;

 		for (it = entries.begin(); it != entries.end(); it++) {
// 			bsaindexentry temp_bie = it->second;
// 			temp += temp_bie.to_string();
// 			temp += ",";
 			}
 	}
 	temp += ">";

	fmt % get_entry_count() % get_offset() % temp % get_file_size();
	return fmt.str();
}

std::map<std::string, bsaindexentry>::iterator bsaindex::begin()
{
  return entries.begin();
}

std::map<std::string, bsaindexentry>::const_iterator bsaindex::begin() const
{
  return entries.begin();
}

std::map<std::string, bsaindexentry>::iterator bsaindex::end()
{
  return entries.end();
}

std::map<std::string, bsaindexentry>::const_iterator bsaindex::end() const
{
  return entries.end();
}

bsaindexentry& bsaindex::operator[] (const std::string& key)
{
    return entries[key];
}

std::list<bsaindexentry>* bsaindex::get_entries(ENTRY_TYPES entry_type) const
{
  std::list<bsaindexentry>* ret = new std::list<bsaindexentry>();

  std::map<std::string, bsaindexentry>::const_iterator it;
  
  for (it = entries.begin(); it!= entries.end(); it++) {
    std::string key = it->first;
    bsaindexentry value = it->second;

    if (key.rfind(entry_type) != std::string::npos) {
      //key ends with entry_type
      ret->push_back(value);
    }
  }

  return ret;
}

std::list<bsaindexentry>* bsaindex::get_voc_entries() const
{
  return get_entries(VOCS);
}

std::list<bsaindexentry>* bsaindex::get_cfa_entries() const
{
  return get_entries(CFAS);
}

std::list<bsaindexentry>* bsaindex::get_dfa_entries() const
{
  return get_entries(DFAS);
}

std::list<bsaindexentry>* bsaindex::get_cif_entries() const
{
  return get_entries(CIFS);
}

void bsaindex::load(std::istream& is)
{
  // Accumulate the starting offset of each file/chunk in the BSA.  Start
  // off with this set to 2, since we need to skip the 2-byte word at the
  // beginning of the file that contains the size of the index table.
  unsigned int local_offset = 2;

  unsigned long index_size = get_entry_count() * SIZE;

  unsigned long file_offset = get_file_size() - index_size;

  is.seekg(file_offset);

  // Scan through the indices in order.  This is also the order in which
  // they are stored in the BSA file, which allows us to determine the
  // offset of the corresponding file/chunk in the BSA.  The index in
  // Global.BSA does not store offsets, only the size of the data.  Since
  // all data is packed without any padding, we need to accumulate the
  // sizes to figure out the offset of each file within Global.BSA. 

  for (unsigned int entry_number = 0; entry_number < get_entry_count(); entry_number++) {
    bsaindexentry* e = new bsaindexentry();
    e->set_offset(local_offset);
    try {
      e->read(is);
    } catch (int e) {
      std::cerr << "Exception in bsaindexentry.read(): " << e << std::endl;
    }

    local_offset += e->get_size();
  
    const std::string key = e->get_name();
    entries.insert( std::pair<std::string, bsaindexentry&>(key, *e) );
    delete e;
    e = NULL;
  }
}

void bsaindex::operator= (const bsaindex& other)
{
  if (this != &other) {
    set_entry_count(other.get_entry_count());
    set_offset(other.get_offset());
    set_file_size(other.get_file_size());
    set_entries(other.get_entries());
  }
}

