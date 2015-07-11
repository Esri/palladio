#pragma once

#include "prt/Object.h"
#include "prt/AttributeMap.h"

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include "boost/filesystem.hpp"
//#pragma GCC diagnostic pop

#include <string>


namespace prt4hdn {
namespace utils {

const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions);
std::string objectToXML(prt::Object const* obj);
void getPathToCurrentModule(boost::filesystem::path& path);
std::string getSharedLibraryPrefix();
std::string getSharedLibrarySuffix();
std::string toOSNarrowFromUTF16(const std::wstring& osWString);
std::wstring toUTF16FromOSNarrow(const std::string& osString);

}
}
