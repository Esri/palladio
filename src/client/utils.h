#pragma once

#include "logging.h"

#include "prt/Object.h"
#include "prt/AttributeMap.h"

#include <string>

namespace boost { namespace filesystem { class path; } }

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

using CacheObjectUPtr           = std::unique_ptr<prt::CacheObject, PRTDestroyer>;
using AttributeMapUPtr          = std::unique_ptr<const prt::AttributeMap, PRTDestroyer>;
using AttributeMapVector        = std::vector<AttributeMapUPtr>;
using AttributeMapBuilderUPtr   = std::unique_ptr<prt::AttributeMapBuilder, PRTDestroyer>;
using AttributeMapBuilderVector = std::vector<p4h::AttributeMapBuilderUPtr>;
using InitialShapeBuilderUPtr   = std::unique_ptr<prt::InitialShapeBuilder, PRTDestroyer>;
using InitialShapeBuilderVector = std::vector<InitialShapeBuilderUPtr>;
using ResolveMapUPtr            = std::unique_ptr<const prt::ResolveMap, PRTDestroyer>;
using RuleFileInfoUPtr          = std::unique_ptr<const prt::RuleFileInfo, PRTDestroyer>;
typedef std::unique_ptr<const prt::EncoderInfo, PRTDestroyer> EncoderInfoPtr;
typedef std::unique_ptr<prt::OcclusionSet, PRTDestroyer> OcclusionSetPtr;

namespace utils {

void getCGBs(const ResolveMapUPtr& rm, std::vector<std::pair<std::wstring,std::wstring>>& cgbs);
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

} // namespace utils
} // namespace p4h
