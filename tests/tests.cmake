# Tests
# This file is included by the root CMakeLists.txt

# Test sources
set(TEST_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/catch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/commandlineexporter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/commandlineparser.h
)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_benchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_canvaspainter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colormanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_defaultlayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_welcomehelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_commandlineexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/commandlineexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_commandlineparser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/commandlineparser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_movieexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_inputsmoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layerbitmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layercamera.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapbucket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_brushpresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_brushpresetdock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_erasertool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_eyedroppertool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_editorimport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_propertyinfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_lassofilltool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_qminiz.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_shortcutfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_shapetool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_stroketool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_tabletapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_toolsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_undoredomanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_vectorimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_viewmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_importlayersdialog.cpp
)

# App sources under test (the dialog and what it links against)
set(TEST_APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/importlayersdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/repositionframesdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/repositionframesdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/importlayersdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/errordialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/errordialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/filedialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/filedialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/app_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/app_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/importlayersdialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/errordialog.ui
)

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
    ${TEST_APP_SOURCES}
    ${TEST_RESOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/shortcutfilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/shortcutfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/defaultlayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/defaultlayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/welcomehelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/welcomehelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/basedockwidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/basedockwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/titlebarwidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/titlebarwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/buttonappearancewatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/buttonappearancewatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/brushpresetdock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/brushpresetdock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/brushpreseteditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/brushpreseteditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/pressurecurvewidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/pressurecurvewidget.cpp
)

# Include directories
target_include_directories(pencil2d_tests PRIVATE
    ${CORE_LIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)

set_target_properties(pencil2d_tests PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/app/ui
)

# Link libraries
target_link_libraries(pencil2d_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Xml
    Qt6::Multimedia
    Qt6::Svg
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
