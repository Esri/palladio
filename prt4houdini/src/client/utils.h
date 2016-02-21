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

struct PRTDestroyer {
	void operator()(prt::Object const* p) {
		if (p)
			p->destroy();
		else
			LOG_WRN << "trying to destroy null prt object!";
	}
};

typedef std::vector<const prt::InitialShape*> InitialShapeNOPtrVector;
typedef std::vector<const prt::AttributeMap*> AttributeMapNOPtrVector;

typedef std::unique_ptr<const prt::AttributeMap, PRTDestroyer> AttributeMapPtr;
typedef std::unique_ptr<prt::AttributeMapBuilder, PRTDestroyer> AttributeMapBuilderPtr;
typedef std::unique_ptr<const prt::InitialShape, PRTDestroyer> InitialShapePtr;
typedef std::unique_ptr<prt::InitialShapeBuilder, PRTDestroyer> InitialShapeBuilderPtr;
typedef std::unique_ptr<const prt::ResolveMap, PRTDestroyer> ResolveMapPtr;
typedef std::unique_ptr<const prt::RuleFileInfo, PRTDestroyer> RuleFileInfoPtr;
typedef std::unique_ptr<const prt::EncoderInfo, PRTDestroyer> EncoderInfoPtr;

namespace utils {


void getCGBs(const ResolveMapPtr& rm, std::vector<std::pair<std::wstring,std::wstring>>& cgbs);
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
