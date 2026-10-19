cmake_minimum_required(VERSION 3.21...3.31)
set(CMAKE_OSX_ARCHITECTURES x86_64;arm64)

# Set project name and version
project(JustAnimate
    VERSION 0.7.0
    DESCRIPTION "JustAnimate Animation Software (based on Pencil2D)"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set version
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "0.0.0.0")
endif()

message(STATUS "App Version: ${APP_VERSION}")

# Find Qt
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    Xml
    Multimedia
    Svg
    Network
    LinguistTools
)

# Global definitions
add_compile_definitions(
    APP_VERSION="${APP_VERSION}"
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_UP_TO=0x050F00
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)

# Git revision for the About dialog and error reports (set at configure time)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_SHA1 OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -n 1 --pretty=format:%cd --date=format:%Y-%m-%d_%H:%M:%S
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_STAMP OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(GIT_SHA1 AND GIT_STAMP)
        add_compile_definitions(GIT_EXISTS GIT_CURRENT_SHA1=${GIT_SHA1} GIT_TIMESTAMP=${GIT_STAMP})
    endif()
endif()

# MSVC specific flags
if(MSVC)
    add_compile_options(/MP)  # Multi-processor compilation
endif()

# Warnings are errors in CI only: a new compiler must not break a user's build
option(JUSTANIMATE_WERROR "Treat compiler warnings as errors" OFF)
if(JUSTANIMATE_WERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

# Include subdirectories to build as single executable
include(core_lib/core_lib.cmake)
include(app/app.cmake)
include(tests/tests.cmake)
include(docs/manual/manual.cmake)

# Packages (cpack): NSIS installer + ZIP on Windows, DMG on macOS; Linux uses an AppImage built in CI
set(CPACK_PACKAGE_VENDOR "JustAnimate")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "JustAnimate")
set(CPACK_PACKAGE_EXECUTABLES justanimate JustAnimate)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
# .pclx/.pcl open in JustAnimate with the exe's embedded icon (version.rc); removed again on uninstall
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
  WriteRegStr HKCR 'JustAnimate.Project' '' 'JustAnimate project'
  WriteRegStr HKCR 'JustAnimate.Project\\\\DefaultIcon' '' '$INSTDIR\\\\justanimate.exe,0'
  WriteRegStr HKCR 'JustAnimate.Project\\\\shell\\\\open\\\\command' '' `\\\"$INSTDIR\\\\justanimate.exe\\\" \\\"%1\\\"`
  WriteRegStr HKCR '.pclx' '' 'JustAnimate.Project'
  WriteRegStr HKCR '.pcl' '' 'JustAnimate.Project'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
  DeleteRegKey HKCR 'JustAnimate.Project'
  DeleteRegKey HKCR '.pclx'
  DeleteRegKey HKCR '.pcl'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
")
set(CPACK_DMG_VOLUME_NAME "JustAnimate")
if(WIN32)
    set(CPACK_GENERATOR NSIS ZIP)
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
endif()
include(CPack)
