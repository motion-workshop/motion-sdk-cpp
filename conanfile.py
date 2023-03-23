from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class MotionSdkRecipe(ConanFile):
    name = "motionsdk"
    version = "4.2.0"
    description = "Motion SDK C++"
    homepage = "https://github.com/motion-workshop/motion-sdk-cpp"
    license = "BSD"

    # Binary configuration
    settings = "os", "arch", "compiler", "build_type"

    # Copy sources to when building this recipe for the local cache
    exports_sources = "CMakeLists.txt", "include/*", "src/*", "example/*", "test/*"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            self.requires("catch2/3.3.2")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["motionsdk"]
