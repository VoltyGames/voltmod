cmake_minimum_required(VERSION 4.4)
project(s2sdk-test LANGUAGES CXX)

find_package(s2sdk CONFIG REQUIRED)

if(NOT COMMAND s2sdk_attach_plugin_support)
    message(FATAL_ERROR "s2sdk shipped no s2sdk-sources.cmake build module")
endif()

add_library(s2sdk-test STATIC src/test.cpp)
target_compile_features(s2sdk-test PRIVATE cxx_std_17)
target_link_libraries(s2sdk-test PRIVATE VoltMod::S2SDK)

s2sdk_attach_generated_sources(s2sdk-test)
s2sdk_attach_engine_sources(s2sdk-test)
