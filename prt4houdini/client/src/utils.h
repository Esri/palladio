#pragma once

#include "prt/Object.h"
#include "prt/AttributeMap.h"

#ifndef WIN32
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include "boost/filesystem.hpp"
#ifndef WIN32
#	pragma GCC diagnostic pop
#endif

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
