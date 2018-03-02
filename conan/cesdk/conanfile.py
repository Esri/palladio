from conans import ConanFile
from conans import tools

# usage: conan create cesdk/1.9.3786@esri-rd-zurich/stable -s compiler=gcc -s compiler.version=4.8

class CESDKConan(ConanFile):
    name        = "cesdk"
    version     = "1.9.3786"
    settings    = "os", "compiler", "arch"
    description = "Develop 3D applications using the procedural geometry engine of Esri CityEngine."
    url         = "https://github.com/Esri/esri-cityengine-sdk"
    license     = "CityEngine EULA"

    baseURL     = "https://github.com/esri/esri-cityengine-sdk/releases/download/{}/esri_ce_sdk-{}-{}-{}-x86_64-rel-opt.zip"

    def build(self):
        if self.settings.os == "Windows":
            url = self.baseURL.format(self.version, self.version, "win7", "vc14")
        elif self.settings.os == "Linux":
            url = self.baseURL.format(self.version, self.version, "rhel6", "gcc48")
        elif self.settings.os == "Macos":
            url = self.baseURL.format(self.version, self.version, "osx11", "ac73")
        else:
            raise Exception("Binary does not exist for this configuration")
        tools.get(url)

    def package(self):
        self.copy("*", ".", "esri_ce_sdk")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
