cmake_minimum_required(VERSION 3.21)

project(phong-cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Quick 3D Helpers provide ExtrudedTextGeometry since 6.9
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick Quick3D Quick3DPhysics)

qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(phong-cpp
    src/main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(phong-cpp
    URI Phong
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Scene.qml
        qml/Text3D.qml
        qml/Disc.qml
        qml/IntroScene.qml
        qml/MenuScene.qml
        qml/GameScene.qml
        qml/SettingsScene.qml
        qml/ResultScene.qml
    SOURCES
        src/player.h src/player.cpp
        src/match.h src/match.cpp
        src/computerplayer.h src/computerplayer.cpp
        src/gamesettings.h src/gamesettings.cpp
)

# The generated QML type registration includes the headers by name
target_include_directories(phong-cpp PRIVATE src)

target_link_libraries(phong-cpp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
)

set_target_properties(phong-cpp PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(EMSCRIPTEN)
    # PhysX and the Qt Quick 3D renderer need more than the default heap
    set_target_properties(phong-cpp PROPERTIES QT_WASM_INITIAL_MEMORY "128MB")
endif()

if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS phong-cpp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
