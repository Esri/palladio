#include "RulePackageFS.h"

#include "prtx/DataBackend.h" // !!! use of PRTX requires palladio_fs to be built with the same compiler as PRT
#include "prtx/URI.h"

#include "FS/FS_ReaderStream.h"

#include <cassert>
#include <iostream>
#include <map>
#include <mutex>

namespace {

const std::string SCHEMA_RPK = "rpk:";

bool isRulePackageURI(const char* p) {
	if (p == nullptr)
		return false;

	// needs to start with rpk: schema
	if (std::strncmp(p, SCHEMA_RPK.c_str(), SCHEMA_RPK.length()) != 0)
		return false;

	// needs to contain '!' separator
	if (std::strchr(p, '!') == nullptr)
		return false;

	return true;
}

// The base URI is the "inner most" URI as defined by prtx::URI, i.e. the actual file
std::string getBaseURIPath(const char* p) {
	// we assume p to be a percent-encoded UTF-8 URI (it comes from a PRT resolve map)
	prtx::URIPtr uri = prtx::URI::create(toUTF16FromUTF8(p));
	if (!uri)
		return {};

	// let's find the innermost URI (the URI could point to a texture inside USDZ inside RPK)
	while (uri->getNestedURI())
		uri = uri->getNestedURI();

	return toUTF8FromUTF16(uri->getPath());
}

prtx::BinaryVectorPtr resolveRulePackageFile(const char* source, prt::Cache* cache) {
	assert(source != nullptr);
	const std::wstring uri = toUTF16FromOSNarrow(source);

	try {
		std::wstring warnings;
		const prtx::BinaryVectorPtr data = prtx::DataBackend::resolveBinaryData(cache, uri, nullptr, &warnings);
		if (!warnings.empty())
			std::wcerr << L"resolveBinaryData warnings: " << warnings << std::endl;
		return data;
	}
	catch (std::exception& e) {
		std::cerr << "caught exception: " << e.what() << std::endl;
	}

	return {};
}

} // namespace

RulePackageReader::RulePackageReader(prt::Cache* cache) : mCache(cache) {
	UTaddAbsolutePathPrefix(SCHEMA_RPK.c_str());
}

FS_ReaderStream* RulePackageReader::createStream(const char* source, const UT_Options*) {
	if (isRulePackageURI(source)) {
		const prtx::BinaryVectorPtr buf = resolveRulePackageFile(source, mCache);
		if (!buf)
			return nullptr;
		UT_WorkBuffer buffer((const char*)buf->data(), buf->size());
		return new FS_ReaderStream(buffer, 0); // freed by Houdini
	}
	return nullptr;
}

RulePackageInfoHelper::RulePackageInfoHelper(prt::Cache* cache) : mCache(cache) {}

bool RulePackageInfoHelper::canHandle(const char* source) {
	return isRulePackageURI(source);
}

bool RulePackageInfoHelper::hasAccess(const char* source, int mode) {
	std::string src(source);
	if (isRulePackageURI(source))
		src = getBaseURIPath(source);
	FS_Info info(src.c_str());
	return info.hasAccess(mode);
}

bool RulePackageInfoHelper::getIsDirectory(const char* source) {
	if (isRulePackageURI(source))
		return false;
	FS_Info info(source);
	return info.getIsDirectory();
}

#if (HOUDINI_VERSION_MAJOR >= 19 && HOUDINI_VERSION_MINOR >= 5)
time_t RulePackageInfoHelper::getModTime(const char* source) {
#else
int RulePackageInfoHelper::getModTime(const char* source) {
#endif
	std::string src(source);
	if (isRulePackageURI(source))
		src = getBaseURIPath(source);
	FS_Info info(src.c_str());
	return info.getModTime();
}

int64 RulePackageInfoHelper::getSize(const char* source) {
	if (isRulePackageURI(source)) {
		const prtx::BinaryVectorPtr buf = resolveRulePackageFile(source, mCache);
		return buf->size();
	}
	FS_Info info(source);
	return info.getFileDataSize();
}

UT_String RulePackageInfoHelper::getExtension(const char* source) {
	if (isRulePackageURI(source)) {
		const char* lastSchemaSep = std::strrchr(source, '.');
		return UT_String(lastSchemaSep);
	}
	FS_Info info(source);
	return info.getExtension();
}

bool RulePackageInfoHelper::getContents(const char* source, UT_StringArray& contents, UT_StringArray* dirs) {
	if (isRulePackageURI(source))
		return false; // we only support individual texture files atm
	FS_Info info(source);
	return info.getContents(contents, dirs);
}
