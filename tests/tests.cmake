# Tests
# This file is included by the root CMakeLists.txt

# Test sources
set(TEST_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/catch.hpp
)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_benchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_canvaspainter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colormanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colorwheel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_defaultlayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_welcomehelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_commandlineexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_commandlineparser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_movieexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_spritesheet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_inputsmoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layerbitmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layercamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layergroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layermanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layersound.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layervector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_onionskin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_pegbaraligner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_playbackmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_pointerevent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_filemanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_emergencysave.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_blendmode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_roughlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_tween.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_mark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_exposure.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_xsheet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapbucket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_brushpresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_brushpresetdock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_erasertool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_eyedroppertool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_editorimport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_propertyinfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_lassofilltool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_movetool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_qminiz.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_rotateviewtool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_scribblearea.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_commandsearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_shortcutfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_shapetool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_stroketool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_tabletapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_toolsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_undoredomanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_vectorimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_viewmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_importlayersdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_checkupdatesdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_appicon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_prefsearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_accessibility.cpp
)

# The whole app minus its entry point, so tests can build MainWindow2.
set(TEST_ALL_APP_SOURCES ${APP_HEADERS} ${APP_SOURCES} ${APP_RESOURCES})
list(REMOVE_ITEM TEST_ALL_APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app/src/main.cpp)

set(TEST_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/tests.qrc
)

# Create test executable with core_lib sources
add_executable(pencil2d_tests
    ${CORE_LIB_HEADERS}
    ${CORE_LIB_SOURCES}
    ${CORE_LIB_OBJCXX_SOURCES}
    ${CORE_LIB_RESOURCES}
    ${TEST_HEADERS}
    ${TEST_SOURCES}
    ${TEST_ALL_APP_SOURCES}
    ${TEST_RESOURCES}
)

# Include directories
target_include_directories(pencil2d_tests PRIVATE
    ${CORE_LIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)

# App headers rely on the precompiled header for their Qt includes.
target_precompile_headers(pencil2d_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/src/corelib-pch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/app-pch.h
)

set_target_properties(pencil2d_tests PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/app/ui
)

target_compile_definitions(pencil2d_tests PRIVATE JUSTANIMATE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Link libraries
target_link_libraries(pencil2d_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Xml
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Network
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(pencil2d_tests PRIVATE ${APPKIT_FRAMEWORK})
endif()
if(WIN32)
    # QWindowsApplication::setWinTabEnabled is private Qt API
    target_link_libraries(pencil2d_tests PRIVATE Qt6::GuiPrivate)
endif()

# Opt-in AddressSanitizer + UndefinedBehaviorSanitizer for the test binary (GCC/Clang).
option(JUSTANIMATE_SANITIZE "Build pencil2d_tests with ASan and UBSan" OFF)
if(JUSTANIMATE_SANITIZE)
    target_compile_options(pencil2d_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(pencil2d_tests PRIVATE -fsanitize=address,undefined)
endif()

# Enable testing
enable_testing()
add_test(NAME pencil2d_tests COMMAND pencil2d_tests)

# Benchmark (ROADMAP 1.1): `cmake --build <build> --target benchmark`.
# Use a Release build for numbers you compare; see docs/benchmarks.md.
add_custom_target(benchmark
    COMMAND pencil2d_tests "[benchmark]"
    DEPENDS pencil2d_tests
    USES_TERMINAL
)

# Brush replay timing (ROADMAP B0): prints one line per tool and size for util/compare_replay.py.
add_custom_target(replay
    COMMAND pencil2d_tests "[.replay]"
    DEPENDS pencil2d_tests
    USES_TERMINAL
)
