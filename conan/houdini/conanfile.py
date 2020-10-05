from conans import ConanFile
from conans.model.version import Version
from conans.errors import ConanInvalidConfiguration
import os


class HoudiniConan(ConanFile):
    name = "houdini"
    settings = "os", "compiler", "arch"
    description = "Houdini is a 3D animation application software developed by Side Effects Software based in Toronto."
    url = "https://www.sidefx.com"
    license = "SIDE EFFECTS SOFTWARE LICENSE AGREEMENT, https://www.sidefx.com/legal/license-agreement"
    short_paths = True

    local_houdini_installation = ""

    def build(self):
        proto_installation_path = r'C:\Program Files\Side Effects Software\Houdini {}' \
            if self.settings.os == "Windows" else "/opt/hfs{}"
        self.local_houdini_installation = os.getenv(
            'HOUDINI_INSTALL') if 'HOUDINI_INSTALL' in os.environ else proto_installation_path.format(self.version)
        if not os.path.exists(self.local_houdini_installation):
            raise ConanInvalidConfiguration("Invalid Houdini installation path: {}".format(self.local_houdini_installation))

    def package(self):
        self.copy("bin/*", ".", self.local_houdini_installation)  # needed for sesitag etc
        self.copy("toolkit/*", ".", self.local_houdini_installation)
        if self.settings.os == "Windows":
            self.copy("custom/*", ".", self.local_houdini_installation)
        elif self.settings.os == "Linux":
            self.copy("houdini_setup*", ".", self.local_houdini_installation)  # needed for sesitag etc
            self.copy("dsolib/*", ".", self.local_houdini_installation)
            self.copy("houdini/Licensing.opt", ".", self.local_houdini_installation)

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
