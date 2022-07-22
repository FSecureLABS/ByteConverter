cmake_minimum_required (VERSION 3.16)

project (ByteConverter)

option(ByteConverterBuildTests "Build unit tests" ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_subdirectory ("src")

## TESTS
if(ByteConverterBuildTests)
	include(CTest)
	add_subdirectory ("test")
endif()
