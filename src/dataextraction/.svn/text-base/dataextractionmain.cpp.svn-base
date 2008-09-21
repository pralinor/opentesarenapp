#include "dataextractionmain.h"

dataextractionmain::dataextractionmain():
  gbsa()
{
}

dataextractionmain::dataextractionmain(const char* gbsa_filename):
  gbsa(gbsa_filename)
{
}

dataextractionmain::dataextractionmain(const std::string& gbsa_filename):
  gbsa(gbsa_filename)
{
}

dataextractionmain::~dataextractionmain()
{
}

globalbsa dataextractionmain::get_globalbsa() const
{
  return gbsa;
}

#if 0
void dataextractionmain::set_globalbsa(const globalbsa& g)
{
  this->gbsa = g;
}
#endif

std::string dataextractionmain::to_string() const
{
	boost::format fmt(
			"dataextractionmain: [globalbsa=%s]");
	fmt % gbsa.to_string();
	return fmt.str();
}
