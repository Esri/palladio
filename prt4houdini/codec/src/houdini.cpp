#include "houdini.h"

#include "prtx/prtx.h"
#include "prtx/ExtensionManager.h"

#include "encoder/HoudiniEncoder.h"


static const int MINIMAL_VERSION_MAJOR = 1;
static const int MINIMAL_VERSION_MINOR = 2;


extern "C" {


PRT4HOUDINI_CODEC_EXPORTS_API void registerExtensionFactories(prtx::ExtensionManager* manager) {
	manager->addFactory(HoudiniEncoderFactory::createInstance());
}


PRT4HOUDINI_CODEC_EXPORTS_API void unregisterExtensionFactories(prtx::ExtensionManager* /*manager*/) {
}


PRT4HOUDINI_CODEC_EXPORTS_API int getMinimalVersionMajor() {
	return MINIMAL_VERSION_MAJOR;
}


PRT4HOUDINI_CODEC_EXPORTS_API int getMinimalVersionMinor() {
	return MINIMAL_VERSION_MINOR;
}


}
