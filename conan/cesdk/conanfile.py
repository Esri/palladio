from conans import ConanFile
from conans import tools
from conans.model.version import Version

class CESDKConan(ConanFile):
    name = "cesdk"
    settings = "os", "compiler", "arch"
    description = "Develop 3D applications using the procedural geometry engine of Esri CityEngine."
    url = "https://github.com/Esri/cityengine-sdk"
    license = "CityEngine EULA"
    baseURL = "https://github.com/esri/cityengine-sdk/releases/download/{}/esri_ce_sdk-{}-{}-{}-x86_64-rel-opt.zip"

    def build(self):
        if self.settings.os == "Windows":
            if self.version >= Version("2.4.7316"):
                toolchain = "vc1427"
            elif self.version >= Version("2.2.6332"):
                toolchain = "vc142"
            else:
                toolchain = "vc141"
            url = self.baseURL.format(self.version, self.version, "win10", toolchain)
        elif self.settings.os == "Linux":
            if self.version >= Version("2.4.7316"):
                gccVersion = "gcc93"
            else:
                gccVersion = "gcc63"
            url = self.baseURL.format(self.version, self.version, "rhel7", gccVersion)
        else:
            raise Exception("Unsupported configuration")
        tools.get(url, verify=False)

    def package(self):
        self.copy("*", dst="esri_ce_sdk")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
