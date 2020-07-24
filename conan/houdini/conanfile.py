from conans import ConanFile
from conans.model.version import Version
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
            self.copy("bin/*", ".", local_install) # needed for sesitag etc
            self.copy("custom/*", ".", local_install)
            self.copy("toolkit/*", ".", local_install)
        elif self.settings.os == "Linux":
            local_install = os.getenv('HOUDINI_INSTALL')\
                if 'HOUDINI_INSTALL' in os.environ\
                else "/opt/hfs{}".format(self.version)
            self.copy("houdini_setup*", ".", local_install) # needed for sesitag etc
            self.copy("bin/*", ".", local_install) # needed for sesitag etc
            self.copy("dsolib/*", ".", local_install)
            self.copy("toolkit/*", ".", local_install)
            self.copy("houdini/Licensing.opt", ".", local_install)
        else:
            raise Exception("platform not supported!")

    def package_info(self):
        self.cpp_info.libdirs = ['dsolib']
        self.cpp_info.libs = ['HoudiniUI', 'HoudiniOPZ', 'HoudiniOP3', 'HoudiniOP2',
                              'HoudiniOP1', 'HoudiniGEO', 'HoudiniPRM', 'HoudiniUT']

    def package_id(self):
        v = Version(str(self.settings.compiler.version))
        if self.settings.compiler == "Visual Studio" and ("15" <= v <= "16"):
            self.info.settings.compiler.version = "Compatible with MSVC 14.1 and 14.2"
        elif self.settings.compiler == "gcc" and ("6" <= v < "9"):
            self.info.settings.compiler.version = "Compatible with GCC 6 tru 8"
