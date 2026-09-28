cmake_minimum_required(VERSION 3.21)

project(phong VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Quick 3D Helpers provide ExtrudedTextGeometry since 6.9
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick Quick3D Quick3DPhysics Quick3DParticles)
# Sound on the desktop, the browser uses Web Audio
if(NOT EMSCRIPTEN)
    find_package(Qt6 QUIET COMPONENTS Multimedia)
endif()

qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(phong
    src/main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(phong
    URI Phong
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Scene.qml
        qml/Text3D.qml
        qml/Disc.qml
        qml/ModifierItem.qml
        qml/BallBody.qml
        qml/PaddleBody.qml
        qml/Sparks.qml
        qml/Banner.qml
        qml/FloatingText.qml
        qml/StatsScene.qml
        qml/GraphicsScene.qml
        qml/OptionsScene.qml
        qml/PhongMaterial.qml
        qml/Starfield.qml
        qml/MenuScene.qml
        qml/GameScene.qml
        qml/SettingsScene.qml
        qml/ResultScene.qml
    SOURCES
        src/player.h src/player.cpp
        src/match.h src/match.cpp
        src/dash.h src/dash.cpp
        src/computerplayer.h src/computerplayer.cpp
        src/modifiers.h src/modifiers.cpp
        src/arenas.h src/arenas.cpp
        src/soundeffects.h src/soundeffects.cpp
        src/stats.h src/stats.cpp
        src/graphicssettings.h src/graphicssettings.cpp
        src/gamesettings.h src/gamesettings.cpp
    RESOURCES
        config/modifiers.json
        config/arenas.json
)

# The generated QML type registration includes the headers by name
target_include_directories(phong PRIVATE src)

target_link_libraries(phong PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
)

if(TARGET Qt6::Multimedia)
    target_link_libraries(phong PRIVATE Qt6::Multimedia)
    target_compile_definitions(phong PRIVATE PHONG_HAVE_MULTIMEDIA)
endif()

set_target_properties(phong PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(EMSCRIPTEN)
    # PhysX and the Qt Quick 3D renderer need more than the default heap
    set_target_properties(phong PROPERTIES QT_WASM_INITIAL_MEMORY "128MB")
endif()

if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS phong
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
