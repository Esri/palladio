#include "RulePackageFS.h"

#include "palladio/Utils.h"

#include "FS/FS_Utils.h" // required for installFSHelpers to work below
#include "UT/UT_DSOVersion.h" // required for valid Houdini DSO

#include <iostream>
#include <memory>

namespace {

CacheObjectUPtr prtCache; // TODO: prevent from growing too much

std::unique_ptr<RulePackageReader> rpkReader;
std::unique_ptr<RulePackageInfoHelper> rpkInfoHelper;

} // namespace

void installFSHelpers() {
	prtCache.reset(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_NONREDUNDANT));

	rpkReader = std::make_unique<RulePackageReader>(prtCache.get());
	rpkInfoHelper = std::make_unique<RulePackageInfoHelper>(prtCache.get());

	std::clog << "Palladio: Registered custom FS reader for Rule Packages.\n";
}
