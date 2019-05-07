from conans import ConanFile
from conans import tools


class CESDKConan(ConanFile):
    name = "cesdk"
    settings = "os", "compiler", "arch"
    description = "Develop 3D applications using the procedural geometry engine of Esri CityEngine."
    url = "https://github.com/Esri/esri-cityengine-sdk"
    license = "CityEngine EULA"
    baseURL = "https://github.com/esri/esri-cityengine-sdk/releases/download/{}/esri_ce_sdk-{}-{}-{}-x86_64-rel-opt.zip"

    def build(self):
        if self.settings.os == "Windows":
            url = self.baseURL.format(self.version, self.version, "win10", "vc141")
        elif self.settings.os == "Linux":
            url = self.baseURL.format(self.version, self.version, "rhel7", "gcc63")
        elif self.settings.os == "Macos":
            url = self.baseURL.format(self.version, self.version, "osx12", "ac81")
        else:
            raise Exception("Unsupported configuration")
        tools.get(url, verify=False)

    def package(self):
        self.copy("*", dst="esri_ce_sdk")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
