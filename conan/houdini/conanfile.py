from conans import ConanFile
import os

class HoudiniConan(ConanFile):
    name = "houdini"
    settings = "os", "compiler", "arch"
    description = "Houdini is a 3D animation application software developed by Side Effects Software based in Toronto."
    url = "https://www.sidefx.com"
    license = "SIDE EFFECTS SOFTWARE LICENSE AGREEMENT, https://www.sidefx.com/legal/license-agreement"
    short_paths = True

    houdiniDefaultInstallationPath = r'C:\Program Files\Side Effects Software\Houdini {}'

    def build(self):
        pass

    def package(self):
        if self.settings.os == "Windows":
            local_install = os.getenv('HOUDINI_INSTALL')\
                if 'HOUDINI_INSTALL' in os.environ\
                else self.houdiniDefaultInstallationPath.format(self.version)
            self.copy("*", ".", local_install)
        elif self.settings.os == "Linux":
            local_install = os.getenv('HOUDINI_INSTALL')\
                if 'HOUDINI_INSTALL' in os.environ\
                else "/opt/hfs{}".format(self.version)
            # the python exclude is a workaround: houdini installs some
            # files in the python subdir as only readable by root
            self.copy("*", ".", local_install, symlinks=True, excludes="python/*")
        elif self.settings.os == "Macos":
            # TODO
            pass
        else:
            raise Exception("platform not supported!")

    def package_info(self):
        self.cpp_info.libdirs = ['dsolib']
        self.cpp_info.libs = ['HoudiniUI', 'HoudiniOPZ', 'HoudiniOP3', 'HoudiniOP2',
                              'HoudiniOP1', 'HoudiniGEO', 'HoudiniPRM', 'HoudiniUT']
