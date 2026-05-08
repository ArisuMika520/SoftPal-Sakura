cmake_minimum_required(VERSION 3.20)
project(SoftPal-Sakura LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "SoftPal-Sakura targets Windows only.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(MSVC)
    # Project-wide compile definitions (these are safe under /clr too).
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    # Native-only flags are added per-target inside src/core to avoid clashing
    # with /clr in the C++/CLI UI subproject.
endif()

option(SOFTPAL_BUILD_UI "Build the C++/CLI WinForms UI (needs MSVC + .NET)" ON)

add_subdirectory(src/core)
if(SOFTPAL_BUILD_UI)
    add_subdirectory(src/ui)
endif()
