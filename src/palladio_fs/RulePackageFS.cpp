#include "RulePackageFS.h"

#include "palladio/Utils.h"

#include "prtx/DataBackend.h" // !!! use of PRTX requires palladio_fs to be built with the same compiler as PRT

#include "FS/FS_ReaderStream.h"

#include <cassert>
#include <iostream>
#include <map>

namespace {

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

pld_time_t getFileModificationTime(const char* path) {
	std::string actualPath(path);
	if (isRulePackageUri(path))
		actualPath = getBaseUriPath(path);
	if (actualPath.empty())
		return {};
	FS_Info info(actualPath.c_str());
	return info.getModTime();
}

} // namespace

RulePackageReader::RulePackageReader(prt::Cache* cache) : mCache(cache) {
	UTaddAbsolutePathPrefix(SCHEMA_RPK);
}

FS_ReaderStream* RulePackageReader::createStream(const char* source, const UT_Options*) {
	if (isRulePackageUri(source)) {
		const prtx::BinaryVectorPtr buf = resolveRulePackageFile(source, mCache);
		if (!buf)
			return nullptr;
		UT_WorkBuffer buffer((const char*)buf->data(), buf->size());
		pld_time_t modTime = getFileModificationTime(source);
		return new FS_ReaderStream(buffer, modTime); // freed by Houdini
	}
	return nullptr;
}

RulePackageInfoHelper::RulePackageInfoHelper(prt::Cache* cache) : mCache(cache) {}

bool RulePackageInfoHelper::canHandle(const char* source) {
	return isRulePackageUri(source);
}

bool RulePackageInfoHelper::hasAccess(const char* source, int mode) {
	std::string src(source);
	if (isRulePackageUri(source))
		src = getBaseUriPath(source);
	FS_Info info(src.c_str());
	return info.hasAccess(mode);
}

bool RulePackageInfoHelper::getIsDirectory(const char* source) {
	if (isRulePackageUri(source))
		return false;
	FS_Info info(source);
	return info.getIsDirectory();
}

pld_time_t RulePackageInfoHelper::getModTime(const char* source) {
	return getFileModificationTime(source);
}

int64 RulePackageInfoHelper::getSize(const char* source) {
	if (isRulePackageUri(source)) {
		const prtx::BinaryVectorPtr buf = resolveRulePackageFile(source, mCache);
		return buf->size();
	}
	FS_Info info(source);
	return info.getFileDataSize();
}

UT_String RulePackageInfoHelper::getExtension(const char* source) {
	if (isRulePackageUri(source)) {
		const char* lastSchemaSep = std::strrchr(source, '.');
		return UT_String(lastSchemaSep);
	}
	FS_Info info(source);
	return info.getExtension();
}

bool RulePackageInfoHelper::getContents(const char* source, UT_StringArray& contents, UT_StringArray* dirs) {
	if (isRulePackageUri(source))
		return false; // we only support individual texture files atm
	FS_Info info(source);
	return info.getContents(contents, dirs);
}
