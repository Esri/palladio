#pragma once

#include "prt/Object.h"
#include "prt/AttributeMap.h"
#include "prt/InitialShape.h"
#include "prt/RuleFileInfo.h"
#include "prt/EncoderInfo.h"
#include "prt/OcclusionSet.h"

#include "GA/GA_Primitive.h"

#include <string>


namespace boost { namespace filesystem { class path; } }

struct PRTDestroyer {
	void operator()(prt::Object const* p) {
		if (p) p->destroy();
	}
};

using PrimitiveNOPtrVector      = std::vector<const GA_Primitive*>;

using ObjectUPtr                = std::unique_ptr<const prt::Object, PRTDestroyer>;
using InitialShapeNOPtrVector   = std::vector<const prt::InitialShape*>;
using AttributeMapNOPtrVector   = std::vector<const prt::AttributeMap*>;
using CacheObjectUPtr           = std::unique_ptr<prt::CacheObject, PRTDestroyer>;
using AttributeMapUPtr          = std::unique_ptr<const prt::AttributeMap, PRTDestroyer>;
using AttributeMapVector        = std::vector<AttributeMapUPtr>;
using AttributeMapBuilderUPtr   = std::unique_ptr<prt::AttributeMapBuilder, PRTDestroyer>;
using AttributeMapBuilderVector = std::vector<AttributeMapBuilderUPtr>;
using InitialShapeBuilderUPtr   = std::unique_ptr<prt::InitialShapeBuilder, PRTDestroyer>;
using InitialShapeBuilderVector = std::vector<InitialShapeBuilderUPtr>;
using ResolveMapUPtr            = std::unique_ptr<const prt::ResolveMap, PRTDestroyer>;
using ResolveMapBuilderUPtr     = std::unique_ptr<prt::ResolveMapBuilder, PRTDestroyer>;
using RuleFileInfoUPtr          = std::unique_ptr<const prt::RuleFileInfo, PRTDestroyer>;
using EncoderInfoUPtr           = std::unique_ptr<const prt::EncoderInfo, PRTDestroyer>;
using OcclusionSetUPtr          = std::unique_ptr<prt::OcclusionSet, PRTDestroyer>;

void getCGBs(const ResolveMapUPtr& rm, std::vector<std::pair<std::wstring,std::wstring>>& cgbs);
const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions);
std::string objectToXML(prt::Object const* obj);

void getLibraryPath(boost::filesystem::path& path, const void* func);
std::string getSharedLibraryPrefix();
std::string getSharedLibrarySuffix();

std::string toOSNarrowFromUTF16(const std::wstring& osWString);
std::wstring toUTF16FromOSNarrow(const std::string& osString);
std::string toUTF8FromOSNarrow(const std::string& osString);

std::wstring toFileURI(const boost::filesystem::path& p);
std::wstring percentEncode(const std::string& utf8String);

inline void replace_all_not_of(std::wstring& s, const std::wstring& allowedChars) {
	std::wstring::size_type pos = 0;
	while (pos < s.size()) {
		pos = s.find_first_not_of(allowedChars, pos);
		if (pos == std::wstring::npos)
			break;
		s[pos++] = L'_';
	}
}
