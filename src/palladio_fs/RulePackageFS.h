#pragma once

#include "FS/FS_Info.h"
#include "FS/FS_Reader.h"

#include "palladio/Utils.h"

/**
 * support for nested file RPK URIs:
 * rpk:file:/path/to/file.rpk!/nested/file.cgb
 */

class RulePackageReader : public FS_ReaderHelper {
public:
	RulePackageReader(prt::Cache* cache);
	virtual ~RulePackageReader() = default;

	FS_ReaderStream* createStream(const char* source, const UT_Options* options) override;

private:
	prt::Cache* mCache;
};

class RulePackageInfoHelper : public FS_InfoHelper {
public:
	RulePackageInfoHelper(prt::Cache* cache);
	virtual ~RulePackageInfoHelper() = default;

	bool canHandle(const char* source) override;
	bool hasAccess(const char* source, int mode) override;
	bool getIsDirectory(const char* source) override;
	int getModTime(const char* source) override;
	int64 getSize(const char* source) override;
	UT_String getExtension(const char* source) override;
	bool getContents(const char* source, UT_StringArray& contents, UT_StringArray* dirs) override;

private:
	prt::Cache* mCache;
};
