cmake_minimum_required (VERSION 3.16)
include(Catch)

project(UnitTest)

add_executable(${PROJECT_NAME}
	"test_case/ChooseBetterSignature.cpp"
	"test_case/CustomTypeSerialization.cpp"
	"test_case/PointerTupleConverterSerialization.cpp"
	"test_case/QualifiersErasure.cpp"
	"test_case/SerializationExceptions.cpp"
	"test_case/SimpleTypeSerialization.cpp"
	"test_case/TupleConverterSerialization.cpp"
	"main.cpp")

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_precompile_headers(${PROJECT_NAME} PRIVATE "include/pch.h")

target_link_libraries(${PROJECT_NAME} PRIVATE ByteConverter CONAN_PKG::catch2)

catch_discover_tests(${PROJECT_NAME})
