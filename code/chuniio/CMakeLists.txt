project(chuniio_pico LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()

IF (NOT WIN32)
    MESSAGE(FATAL_ERROR "chuniio.dll must be built for Windows (it's loaded in-process by the game).")
ENDIF()

# Chusan (and segatools games generally) are 32-bit processes. A DLL built
# for the wrong bitness fails to load with HRESULT 0x800700c1
# (ERROR_BAD_EXE_FORMAT) -- if you hit that, this is why. Force 32-bit here
# so it's not left to whatever the ambient toolchain/generator defaults to.
IF (MSVC)
    IF (NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32" AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
        MESSAGE(WARNING "Building with MSVC: make sure you selected a Win32 (x86) generator/platform, e.g. cmake -B build -A Win32")
    ENDIF()
ELSE()
    # MinGW: force -m32 and 32-bit linker flags explicitly.
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
ENDIF()

ADD_LIBRARY(chuniio SHARED
    chuniio.c
    serial_link.c)

TARGET_LINK_LIBRARIES(chuniio setupapi)

SET_TARGET_PROPERTIES(chuniio PROPERTIES
    OUTPUT_NAME "chuniio"
    PREFIX "")