# -----------------------------------------------------
#   CMake build for v32opt, the Vircon32 assembly optimizer
#
#   Laid out after the Vircon32 DevTools' own CMake setup
#   (ComputerSoftware/DevelopmentTools/CMakeLists.txt).
#   This is an ALTERNATIVE to the base Makefile, which
#   keeps working exactly as before; the two never share
#   build products. Build out of tree:
#
#       mkdir build && cd build
#       cmake ..                 (MSYS2/MinGW: cmake -G "MSYS Makefiles" ..)
#       cmake --build .
#       cmake --install .        (may need sudo / an elevated shell)
#
#   Default install locations:
#       Linux / macOS : /usr/local/bin/v32opt
#                       /usr/local/share/man/man1/v32opt.1
#       Windows       : <Program Files>/Vircon32/DevTools/v32opt.exe
#                       (next to compile.exe / assemble.exe, the folder
#                        the Vircon32 DevTools install to), with the
#                        README and manual page in DevTools/docs/v32opt
#   Change the root with -DCMAKE_INSTALL_PREFIX=<dir> (or
#   `cmake --install . --prefix <dir>`).
# -----------------------------------------------------

cmake_minimum_required(VERSION 3.13...3.31)

# Refuse an in-source build: CMake's generated Makefile would overwrite
# the project's own hand-written base Makefile.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "v32opt: in-source CMake builds are not supported -- they would "
        "overwrite the base Makefile. Build in a separate directory:\n"
        "    mkdir build && cd build && cmake ..\n"
        "First remove the CMakeCache.txt and CMakeFiles/ just created here.")
endif()

# By default, project configuration will be Release
# (must be done before project() statement)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# -----------------------------------------------------
#   VERSION (single source: inc/v32opt.h)
# -----------------------------------------------------

# The full version string, e.g. "20261002-dev", read from the same
# #define that `v32opt --version` prints and `make version` stamps into
# the man page. Re-run configuration automatically when it changes.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/inc/v32opt.h" V32OPT_VERSION_LINE
     REGEX "^#define[ \t]+VERSION[ \t]+\"[^\"]*\"")
string(REGEX REPLACE "^#define[ \t]+VERSION[ \t]+\"([^\"]*)\".*" "\\1"
       V32OPT_VERSION "${V32OPT_VERSION_LINE}")
if(NOT V32OPT_VERSION)
    message(FATAL_ERROR "v32opt: could not read VERSION from inc/v32opt.h")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/inc/v32opt.h")

# project() only takes a numeric version: derive YYYY.MM.DD from the
# leading date (the "-dev"/"-release" status is kept in V32OPT_VERSION).
if(V32OPT_VERSION MATCHES "^([0-9][0-9][0-9][0-9])([0-9][0-9])([0-9][0-9])")
    set(V32OPT_NUMERIC_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else()
    set(V32OPT_NUMERIC_VERSION "0.0.0")
endif()

# -----------------------------------------------------
#   DEFINE THE PROJECT
# -----------------------------------------------------

project(v32opt
        VERSION ${V32OPT_NUMERIC_VERSION}
        DESCRIPTION "Assembly optimizer for the Vircon32 fantasy console"
        HOMEPAGE_URL "https://github.com/wedge1020/v32opt"
        LANGUAGES C)

# -----------------------------------------------------
#   IDENTIFY HOST ENVIRONMENT
# -----------------------------------------------------

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_BITS "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TARGET_BITS "32")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TARGET_OS "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_OS "linux")
else()
    set(TARGET_OS "${CMAKE_SYSTEM_NAME}")
endif()

# v32opt uses getopt_long() and strcasecmp(), which MinGW, glibc, musl,
# macOS and the BSDs all provide but Visual C++ does not. The Vircon32
# DevTools are likewise built with MSYS2 + MinGW on Windows.
include(CheckIncludeFile)
check_include_file(getopt.h V32OPT_HAVE_GETOPT_H)
if(NOT V32OPT_HAVE_GETOPT_H)
    message(FATAL_ERROR
        "v32opt: <getopt.h> not found. v32opt needs a POSIX-style C "
        "library (getopt_long, strcasecmp). On Windows, build with "
        "MSYS2 + MinGW, e.g. from a MinGW shell:\n"
        "    cmake -G \"MSYS Makefiles\" ..")
endif()

# -----------------------------------------------------
#   INSTALL LOCATIONS
# -----------------------------------------------------

# On Windows, CMake's default prefix would be "<Program Files>/v32opt".
# Install alongside the Vircon32 DevTools instead ("<Program Files>/
# Vircon32/DevTools", their default location), so v32opt.exe sits next to
# compile.exe and assemble.exe and is found on the same PATH entry.
# An explicit -DCMAKE_INSTALL_PREFIX always wins.
# (Only when the default has the usual "<Program Files>/v32opt" shape --
# the parent is then whichever of "Program Files" or "Program Files (x86)"
# CMake picked for this toolchain, exactly as for the DevTools.)
if(TARGET_OS STREQUAL "windows" AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    get_filename_component(V32OPT_PREFIX_NAME   "${CMAKE_INSTALL_PREFIX}" NAME)
    get_filename_component(V32OPT_PROGRAM_FILES "${CMAKE_INSTALL_PREFIX}" DIRECTORY)
    if(V32OPT_PREFIX_NAME STREQUAL PROJECT_NAME)
        set(CMAKE_INSTALL_PREFIX "${V32OPT_PROGRAM_FILES}/Vircon32" CACHE PATH
            "Install path prefix, prepended onto install directories." FORCE)
    endif()
endif()

include(GNUInstallDirs)   # bin, share/man (CMAKE_INSTALL_PREFIX defaults to /usr/local on Unix)

if(TARGET_OS STREQUAL "windows")
    set(V32OPT_DEFAULT_BINDIR "DevTools")
    set(V32OPT_DEFAULT_MANDIR "DevTools/docs/v32opt")
    set(V32OPT_DEFAULT_DOCDIR "DevTools/docs/v32opt")
else()
    set(V32OPT_DEFAULT_BINDIR "${CMAKE_INSTALL_BINDIR}")
    set(V32OPT_DEFAULT_MANDIR "${CMAKE_INSTALL_MANDIR}/man1")
    set(V32OPT_DEFAULT_DOCDIR "")   # Unix: the man page is the documentation
endif()

set(V32OPT_INSTALL_BINDIR "${V32OPT_DEFAULT_BINDIR}" CACHE STRING
    "Where the v32opt executable is installed (relative to CMAKE_INSTALL_PREFIX)")
set(V32OPT_INSTALL_MANDIR "${V32OPT_DEFAULT_MANDIR}" CACHE STRING
    "Where the v32opt.1 manual page is installed (relative to CMAKE_INSTALL_PREFIX)")
set(V32OPT_INSTALL_DOCDIR "${V32OPT_DEFAULT_DOCDIR}" CACHE STRING
    "Where README.md is installed (relative to CMAKE_INSTALL_PREFIX; empty = not installed)")

# -----------------------------------------------------
#   SOURCE FILES
# -----------------------------------------------------

# Same set the base Makefile builds (src/Makefile's CORE_SRCS plus every
# src/peephole/*.c). CONFIGURE_DEPENDS re-globs when a pass is added.
set(V32OPT_CORE_SRC
    src/main.c
    src/args.c
    src/tools.c
    src/cfg.c
    src/stack.c
    src/dce.c
    src/cse.c
    src/inline.c
    src/promote.c
    src/helpers.c
    src/defines.c
    src/liveness.c)

file(GLOB V32OPT_PEEPHOLE_SRC CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/peephole/*.c")

file(GLOB V32OPT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h")

# -----------------------------------------------------
#   EXECUTABLE
# -----------------------------------------------------

add_executable(v32opt ${V32OPT_CORE_SRC} ${V32OPT_PEEPHOLE_SRC} ${V32OPT_HEADERS})

target_include_directories(v32opt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

# C99 with the platform's extensions (gnu99): strcasecmp/strdup are
# declared by the system headers in that mode.
set_target_properties(v32opt PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON)

# Warning flags, matching src/Makefile where the compiler supports them
include(CheckCCompilerFlag)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(v32opt PRIVATE -Wall -Wextra)
    check_c_compiler_flag(-Wno-format-truncation V32OPT_HAVE_WNO_FORMAT_TRUNCATION)
    if(V32OPT_HAVE_WNO_FORMAT_TRUNCATION)
        target_compile_options(v32opt PRIVATE -Wno-format-truncation)
    endif()
endif()

# libm, where it is a separate library (src/Makefile links -lm too)
if(UNIX AND NOT APPLE)
    target_link_libraries(v32opt PRIVATE m)
endif()

# MinGW: link the C runtime support statically, so the installed
# v32opt.exe needs no MinGW DLLs next to it.
option(V32OPT_STATIC_MINGW "Statically link MinGW runtime libraries" ON)
if(MINGW AND V32OPT_STATIC_MINGW)
    target_link_options(v32opt PRIVATE -static)
endif()

# -----------------------------------------------------
#   SHOW BUILD INFORMATION IN PRETTY FORMAT
# -----------------------------------------------------

message(STATUS "******** v32opt ********")
message(STATUS "Version: ${V32OPT_VERSION}")
if(NOT TARGET_OS STREQUAL "mac")
    message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit")
else()
    message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit (SDK: ${CMAKE_OSX_SYSROOT})")
endif()
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type: (chosen at build time)")
else()
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS " * executable  -> ${V32OPT_INSTALL_BINDIR}")
message(STATUS " * manual page -> ${V32OPT_INSTALL_MANDIR}")
if(V32OPT_INSTALL_DOCDIR)
    message(STATUS " * README      -> ${V32OPT_INSTALL_DOCDIR}")
endif()

# -----------------------------------------------------
#   DEFINE THE INSTALL PROCESS
# -----------------------------------------------------

install(TARGETS v32opt
        RUNTIME DESTINATION "${V32OPT_INSTALL_BINDIR}"
        COMPONENT binaries)

install(FILES man/v32opt.1
        DESTINATION "${V32OPT_INSTALL_MANDIR}"
        COMPONENT documentation)

if(V32OPT_INSTALL_DOCDIR)
    install(FILES README.md
            DESTINATION "${V32OPT_INSTALL_DOCDIR}"
            COMPONENT documentation)
endif()

# `cmake --build . --target uninstall` removes whatever the last install
# put down (CMake has no built-in uninstall; this replays
# install_manifest.txt).
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY)
    add_custom_target(uninstall
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        COMMENT "Uninstalling v32opt")
endif()

# -----------------------------------------------------
#   SMOKE TESTS (ctest)
# -----------------------------------------------------

# Light checks that need no Vircon32 tools: the binary reports the
# expected version, and every unit-test input under testing/ optimizes at
# -O3 in both language modes without error. The full unit suite (which
# assembles and packs both versions with the Vircon32 DevTools) is still
# `make tests` in the base Makefile.
option(BUILD_TESTING "Define the ctest smoke tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_test(NAME version COMMAND v32opt --version)
    set_tests_properties(version PROPERTIES
        PASS_REGULAR_EXPRESSION "${V32OPT_VERSION}")

    file(GLOB V32OPT_TEST_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/testing/*/*.asm")
    foreach(input ${V32OPT_TEST_INPUTS})
        get_filename_component(unit "${input}" NAME_WE)
        if(unit MATCHES "Opt$")
            continue()
        endif()
        foreach(mode c lua)
            add_test(NAME "O3-${mode}-${unit}"
                     COMMAND v32opt "${input}" -O3 -L ${mode}
                             -o "${CMAKE_CURRENT_BINARY_DIR}/testing/${unit}.${mode}.asm")
        endforeach()
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/testing")
endif()

# -----------------------------------------------------
#   DEFINE THE PACKAGE CREATION WITH CPACK
# -----------------------------------------------------

# `cpack` (from the build directory) makes binary packages, as the
# Vircon32 DevTools do: .deb/.rpm/.tar.gz on Linux, .tar.gz on macOS,
# .zip on Windows. Packages carry the full version string in their file
# name; the package-manager version is the numeric YYYY.MM.DD date.
set(CPACK_PACKAGE_NAME "v32opt")
set(CPACK_PACKAGE_VENDOR "Matthew Haas")
set(CPACK_PACKAGE_CONTACT "Matthew Haas")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Assembly optimizer for the Vircon32 fantasy console")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/wedge1020/v32opt")
set(CPACK_PACKAGE_FILE_NAME "v32opt-${V32OPT_VERSION}-${TARGET_OS}${TARGET_BITS}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(TARGET_OS STREQUAL "linux")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthew Haas")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_GENERATOR "TGZ")
    find_program(V32OPT_DPKG_DEB dpkg-deb)
    if(V32OPT_DPKG_DEB)
        list(APPEND CPACK_GENERATOR "DEB")
    endif()
    find_program(V32OPT_RPMBUILD rpmbuild)
    if(V32OPT_RPMBUILD)
        list(APPEND CPACK_GENERATOR "RPM")
    endif()
elseif(TARGET_OS STREQUAL "windows")
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
