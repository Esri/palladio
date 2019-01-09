import os
from conans import ConanFile


class PalladioConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "houdini/[>16.5.0,<17.0.0]@sidefx/stable",\
               "boost/1.66.0@conan/stable", \
               "catch2/2.0.1@bincrafters/stable"
    generators = "cmake"
    default_options = "boost:shared=False"

    def requirements(self):
        if "PLD_CONAN_SKIP_CESDK" not in os.environ:
            if self.settings.os == "Windows":
                self.requires("cesdk/1.9.3786p1@esri-rd-zurich/stable")  # see issue #88
            else:
                self.requires("cesdk/1.9.3786@esri-rd-zurich/stable")

    def configure(self):
        if self.settings.os == "Linux":
            self.options["boost"].fPIC = True
