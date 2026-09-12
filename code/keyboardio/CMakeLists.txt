cmake_minimum_required(VERSION 3.10)
project(keyboardio LANGUAGES C)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()

IF (NOT WIN32)
    MESSAGE(FATAL_ERROR "keyboardio must be built for Windows.")
ENDIF()

ADD_EXECUTABLE(keyboardio
    main.c
    ../chuniio/serial_link.c)

TARGET_INCLUDE_DIRECTORIES(keyboardio PRIVATE ../chuniio)
TARGET_LINK_LIBRARIES(keyboardio setupapi user32)
