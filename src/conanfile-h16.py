import os
from conans import ConanFile


class PalladioConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    default_options = "boost:shared=False"

    def requirements(self):
        self.requires("boost/1.66.0@conan/stable")
        self.requires("catch2/2.0.1@bincrafters/stable")

        if "PLD_CONAN_HOUDINI_VERSION" in os.environ:
            self.requires("houdini/{}@sidefx/stable".format(os.environ["PLD_CONAN_HOUDINI_VERSION"]))
        else:
            self.requires("houdini/[>16.5.0,<17.0.0]@sidefx/stable")

        if "PLD_CONAN_SKIP_CESDK" not in os.environ:
            if "PLD_CONAN_CESDK_VERSION" in os.environ:
                cesdk_version = os.environ["PLD_CONAN_CESDK_VERSION"]
            else:
                cesdk_version = "2.1.5704"
            self.requires("cesdk/{}@esri-rd-zurich/stable".format(cesdk_version))

    def configure(self):
        if self.settings.os == "Linux":
            self.options["boost"].fPIC = True
