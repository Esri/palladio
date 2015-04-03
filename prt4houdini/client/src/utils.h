#ifndef __P4H_UTILS__
#define __P4H_UTILS__

#include "prt/Object.h"
#include "prt/AttributeMap.h"

#include "boost/filesystem.hpp"

#include <string>


namespace prt4hdn {
namespace utils {

const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions);
std::string objectToXML(prt::Object const* obj);
void getPathToCurrentModule(boost::filesystem::path& path);
std::string getSharedLibraryPrefix();
std::string getSharedLibrarySuffix();

}
}

#endif
