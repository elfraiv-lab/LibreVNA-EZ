cmake_minimum_required(VERSION 3.21)
project(LibreVNA-GUI VERSION 1.6.5 LANGUAGES C CXX)

# moc 6.11 (MinGW) crashes when it parses libstdc++ headers; keep them away from it
set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)

# --- sources: everything in this directory except build folders ------------
file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.ui)
list(FILTER GUI_SOURCES EXCLUDE REGEX "/build[^/]*/")
list(FILTER GUI_SOURCES EXCLUDE REGEX "/qtest/")
list(FILTER GUI_SOURCES EXCLUDE REGEX "/Tools/Eigen/")
list(FILTER GUI_SOURCES EXCLUDE REGEX "/changed/")

set(GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/librevna.qrc)

set(EMBEDDED_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../VNA_embedded/Application/Communication/Protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../VNA_embedded/Application/Communication/Protocol.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../VNA_embedded/Application/Communication/PacketConstants.h)

if(WIN32)
    set(GUI_RC ${CMAKE_CURRENT_SOURCE_DIR}/resources/librevna.rc)
    if(NOT EXISTS ${GUI_RC})
        file(WRITE ${GUI_RC} "IDI_ICON1 ICON DISCARDABLE \"librevna.ico\"\n")
    endif()
endif()

qt_add_executable(LibreVNA-GUI WIN32 MACOSX_BUNDLE
    ${GUI_SOURCES} ${GUI_RESOURCES} ${EMBEDDED_HEADERS} ${GUI_RC})

# .ui files are included from other directories (e.g. ui_main.h from SpectrumAnalyzer/),
# so AUTOUIC must search every folder that contains .ui files
set(UI_DIRS "")
foreach(f ${GUI_SOURCES})
    if(f MATCHES "\\.ui$")
        get_filename_component(d ${f} DIRECTORY)
        list(APPEND UI_DIRS ${d})
    endif()
endforeach()
list(REMOVE_DUPLICATES UI_DIRS)
set_target_properties(LibreVNA-GUI PROPERTIES AUTOUIC_SEARCH_PATHS "${UI_DIRS}")

target_include_directories(LibreVNA-GUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools)

target_compile_definitions(LibreVNA-GUI PRIVATE
    FW_MAJOR=1 FW_MINOR=6 FW_PATCH=5 FW_SUFFIX=""
    GITHASH="local")

# upstream .pro removes these (Windows API is used with char strings)
if(WIN32)
    target_compile_definitions(LibreVNA-GUI PRIVATE QMICROZ_LIBRARY)
    qt_disable_unicode_defines(LibreVNA-GUI)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(LibreVNA-GUI PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated -Wno-deprecated-declarations -Wno-deprecated-copy>)
endif()

# --- libusb ------------------------------------------------------------------
# Windows: expects libusb-1.0.a next to this file (as upstream CI does) and
# headers in <Qt>/include/libusb-1.0 or in LIBUSB_INCLUDE_DIR.
if(WIN32)
    set(LIBUSB_INCLUDE_DIR "" CACHE PATH "Folder that contains libusb-1.0/libusb.h")
    if(LIBUSB_INCLUDE_DIR)
        target_include_directories(LibreVNA-GUI PRIVATE ${LIBUSB_INCLUDE_DIR})
    endif()
    target_link_directories(LibreVNA-GUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(LibreVNA-GUI PRIVATE usb-1.0)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    target_link_libraries(LibreVNA-GUI PRIVATE PkgConfig::LIBUSB)
endif()

target_link_libraries(LibreVNA-GUI PRIVATE Qt6::Widgets Qt6::Network Qt6::Svg)
