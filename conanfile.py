from conans import ConanFile, CMake

class ByteConverterConan(ConanFile):
    name = "ByteConverter"
    license = "Copyright F-Secure"
    url = "https://github.com/FSecureLABS/ByteConverter"
    description = """
A header-only library for lightweight serialization
// beware of cross-platform issues
"""
    generators = "cmake"
    exports_sources = "CMakeLists.txt", "src/*", "test/*"
    no_copy_source=True
    options = {
        "build_tests": [True, False],
    }
    default_options = {
        "build_tests": False
    }

    _cmake = None

    def build_requirements(self):
        if self.options.build_tests:
            self.build_requires("catch2/[>=2.13.4]")

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = CMake(self)
        self._cmake.definitions["ByteConverterBuildTests"] = self.options.build_tests
        self._cmake.configure()
        return self._cmake

    def build(self):
        if self.options.build_tests:
            cmake = self._configure_cmake()
            cmake.build()
            cmake.test()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_id(self):
        self.info.header_only()
