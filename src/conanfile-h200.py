import os
from conans import ConanFile


class PalladioConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"

    def requirements(self):
        self.requires("catch2/2.13.7")

        if "PLD_CONAN_HOUDINI_VERSION" in os.environ:
            self.requires("houdini/{}@sidefx/stable".format(os.environ["PLD_CONAN_HOUDINI_VERSION"]))
        else:
            self.requires("houdini/[>20.0.0 <20.5.0]@sidefx/stable")

        if "PLD_CONAN_SKIP_CESDK" not in os.environ:
            if "PLD_CONAN_CESDK_VERSION" in os.environ:
                cesdk_version = os.environ["PLD_CONAN_CESDK_VERSION"]
            else:
                cesdk_version = "3.2.10211"
            self.requires("cesdk/{}@esri-rd-zurich/stable".format(cesdk_version))
