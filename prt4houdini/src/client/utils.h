#pragma once

#include "client/logging.h"

#include "prt/Object.h"
#include "prt/AttributeMap.h"

#ifndef _WIN32
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include "boost/filesystem.hpp"
#ifndef _WIN32
#	pragma GCC diagnostic pop
#endif

#include <string>


namespace p4h {
namespace utils {

struct PRTDestroyer {
	void operator()(prt::Object const* p) {
		if (p)
			p->destroy();
		else
			LOG_WRN << "trying to destroy null prt object!";
	}
};

const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions);
std::string objectToXML(prt::Object const* obj);

void getPathToCurrentModule(boost::filesystem::path& path);
std::string getSharedLibraryPrefix();
std::string getSharedLibrarySuffix();

std::string toOSNarrowFromUTF16(const std::wstring& osWString);
std::wstring toUTF16FromOSNarrow(const std::string& osString);
std::string toUTF8FromOSNarrow(const std::string& osString);

std::wstring toFileURI(const boost::filesystem::path& p);
std::wstring percentEncode(const std::string& utf8String);

}
}
