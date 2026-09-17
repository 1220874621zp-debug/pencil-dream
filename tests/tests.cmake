# Tests
# This file is included by the root CMakeLists.txt

# Test sources
set(TEST_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/catch.hpp
)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colormanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layerlayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_inbetween.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layerbitmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layercamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layermanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layersound.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layervideo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_filemanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colorizeengine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_mlswarp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_deformtool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_bitmapbucket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_clearframe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_propertyinfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_qminiz.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_toolsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_brushengine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_viewmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_flowlayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_onionalign.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_holefiller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_colortoalpha.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_layersplitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/test_scripting.cpp
    # 脚本系统（app 层）：测试直接编入 ScriptHost 实现
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/scriptapi.cpp
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
    ${TEST_RESOURCES}
)

# Include directories
target_include_directories(pencil2d_tests PRIVATE
    ${CORE_LIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)

# Link libraries
target_link_libraries(pencil2d_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Xml
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Qml # QJSEngine（脚本系统测试）
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(pencil2d_tests PRIVATE ${APPKIT_FRAMEWORK})
endif()

# Enable testing
enable_testing()
add_test(NAME pencil2d_tests COMMAND pencil2d_tests)
