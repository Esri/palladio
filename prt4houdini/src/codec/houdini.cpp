#include "houdini.h"

#include "encoder/HoudiniEncoder.h"

#include "prtx/prtx.h"
#include "prtx/ExtensionManager.h"


namespace {
const int VERSION_MAJOR = PRT_VERSION_MAJOR;
const int VERSION_MINOR = PRT_VERSION_MINOR;
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
