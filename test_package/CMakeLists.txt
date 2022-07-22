cmake_minimum_required(VERSION 3.16)

project(Example)

# Conan have issue with VS2022
set(CONAN_DISABLE_CHECK_COMPILER TRUE)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(${PROJECT_NAME} example.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})
