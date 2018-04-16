from conans import ConanFile
from conans import tools

# usage: conan create -s compiler=gcc -s compiler.version=4.8 . cesdk/1.9.3786@esri-rd-zurich/stable


class CESDKConan(ConanFile):
    name = "cesdk"
    version = "1.9.3786"
    settings = "os", "compiler", "arch"
    description = "Develop 3D applications using the procedural geometry engine of Esri CityEngine."
    url = "https://github.com/Esri/esri-cityengine-sdk"
    license = "CityEngine EULA"

    baseURL = "https://github.com/esri/esri-cityengine-sdk/releases/download/{}/esri_ce_sdk-{}-{}-{}-x86_64-rel-opt.zip"

    # we host a manually patched CE SDK for windows to fix loading issues of the codecs dll
    # (see comment on v1.1 github release page)
    baseURLWindows = 'https://github.com/Esri/palladio/releases/download/v1.1/esri_ce_sdk-{}-{}-{}-x86_64-rel-opt-patched-alembic.zip'

    def build(self):
        if self.settings.os == "Windows":
            print("Note: Downloading a customized CE SDK build! See issue #88.")
            url = self.baseURLWindows.format(self.version, "win7", "vc14")
        elif self.settings.os == "Linux":
            url = self.baseURL.format(self.version, self.version, "rhel6", "gcc48")
        elif self.settings.os == "Macos":
            url = self.baseURL.format(self.version, self.version, "osx11", "ac73")
        else:
            raise Exception("Binary does not exist for this configuration")
        tools.get(url)

        # hack: both cesdk and houdini provide libAlembic.so which are not compatible
        #       the alembic library from houdini is loaded first and this prevents libcom.esri.prt.codecs.so to load
        #       we are using a using patchelf to rename libAlembic.so of cesdk
        # note: this requires patchelf 0.9 or later (https://github.com/NixOS/patchelf)
        if self.settings.os == "Linux":
            abc_old = 'Alembic'
            abc_new = 'AlembicCESDK'
            codecs_lib = 'esri_ce_sdk/lib/libcom.esri.prt.codecs.so'
            self.run('cd esri_ce_sdk/lib && mv lib{}.so lib{}.so'.format(abc_old, abc_new))
            self.run('patchelf --replace-needed lib{}.so lib{}.so {}'.format(abc_old, abc_new, codecs_lib))
            tools.replace_in_file('esri_ce_sdk/cmake/prtConfig.cmake', abc_old, abc_new)

    def package(self):
        self.copy("*", ".", "esri_ce_sdk")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
