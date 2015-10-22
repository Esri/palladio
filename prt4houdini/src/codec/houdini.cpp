#include "houdini.h"

#include "prtx/prtx.h"
#include "prtx/ExtensionManager.h"

#include "encoder/HoudiniEncoder.h"

namespace {
const int VERSION_MAJOR = 1;
const int VERSION_MINOR = 4; // TODO: pull from prtVersion file
}

extern "C" {


PRT4HOUDINI_CODEC_EXPORTS_API void registerExtensionFactories(prtx::ExtensionManager* manager) {
	manager->addFactory(HoudiniEncoderFactory::createInstance());
}


PRT4HOUDINI_CODEC_EXPORTS_API void unregisterExtensionFactories(prtx::ExtensionManager* /*manager*/) {
}


PRT4HOUDINI_CODEC_EXPORTS_API int getVersionMajor() {
	return VERSION_MAJOR;
}


PRT4HOUDINI_CODEC_EXPORTS_API int getVersionMinor() {
	return VERSION_MINOR;
}


}
