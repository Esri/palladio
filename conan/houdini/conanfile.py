from conans import ConanFile
from conans import tools

# this recipe creates a houdini conan package from a locally installed houdini
# usage: conan create houdini/16.0.XXX@sidefx/stable -s compiler=gcc -s compiler.version=4.8 -o houdini-flavor=apprentice

class HoudiniConan(ConanFile):
    name        = "houdini"
    settings    = "os", "compiler", "build_type", "arch"
    options     = { "houdini-flavor": ["apprentice", "indie", "core", "fx"] }
    description = "Houdini is a 3D animation application software developed by Side Effects Software based in Toronto."
    url         = "https://www.sidefx.com/products/houdini-apprentice/"
    license     = "SIDE EFFECTS SOFTWARE LICENSE AGREEMENT, https://www.sidefx.com/legal/license-agreement"

    def build(self):
        pass

    def package(self):
        if self.settings.os == "Windows":
            # TODO
            pass
        elif self.settings.os == "Linux":
            local_install = "/opt/hfs{}".format(self.version)
            self.copy("*", ".", local_install, symlinks=True, excludes="python/*")
            # the python exclude is a hacky workaround: houdini installs some files in the python subdir as only readable by root
        elif self.settings.os == "Macos":
            # TODO
            pass
        else:
            raise Exception("platform not supported!")

    def package_info(self):
        self.cpp_info.libdirs = ['dsolib']
        self.cpp_info.libs = [ 'HoudiniUI', 'HoudiniOPZ', 'HoudiniOP3', 'HoudiniOP2', 'HoudiniOP1', 'HoudiniGEO', 'HoudiniPRM', 'HoudiniUT']
        # TODO: provide cpp flags required by houdini
        # TODO: raise conan github issue to get version info by default
        self.user_info.version = self.version
