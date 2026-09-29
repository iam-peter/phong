cmake_minimum_required(VERSION 3.21)

project(phong VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Quick 3D Helpers provide ExtrudedTextGeometry since 6.9
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick Quick3D Quick3DPhysics Quick3DParticles
                                           Network WebSockets)
# Sound and gamepads on the desktop, the browser uses Web Audio and the
# Gamepad API
option(PHONG_FETCH_SDL3 "Download and build SDL 3 for gamepads if it isn't installed" ON)
if(NOT EMSCRIPTEN)
    find_package(Qt6 QUIET COMPONENTS Multimedia)
    find_package(SDL3 CONFIG QUIET COMPONENTS SDL3)

    # Few distributions package SDL 3 yet. The game only needs its gamepads,
    # a static library without audio and video.
    if(NOT TARGET SDL3::SDL3 AND PHONG_FETCH_SDL3)
        include(FetchContent)
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.30/SDL3-3.2.30.tar.gz
            URL_HASH SHA256=4c3b09330d866dc52eb65b66259a6684ad387252ca8c57901b3a2b534eb42e3d
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(SDL_SHARED OFF)
        set(SDL_STATIC ON)
        set(SDL_TEST_LIBRARY OFF)
        set(SDL_EXAMPLES OFF)
        set(SDL_DISABLE_INSTALL ON)
        # Without video SDL needs to know no windows are wanted
        set(SDL_UNIX_CONSOLE_BUILD ON)
        set(sdl_subsystems AUDIO GPU RENDER CAMERA HAPTIC POWER DIALOG)
        # On macOS SDL 3.2 builds its tray only with video and has no
        # stand-in, so without video the link fails. SDL_Init() still
        # leaves the windows to Qt, it only starts gamepads.
        if(NOT APPLE)
            list(APPEND sdl_subsystems VIDEO)
        endif()
        foreach(subsystem IN LISTS sdl_subsystems)
            set(SDL_${subsystem} OFF)
        endforeach()
        message(STATUS "Fetching SDL 3 for gamepads")
        FetchContent_MakeAvailable(SDL3)
        set(SDL3_VERSION 3.2.30)
    endif()
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
        qml/PlayerPanel.qml
        qml/PowerBar.qml
        qml/Sparks.qml
        qml/AchievementsScene.qml
        qml/Banner.qml
        qml/BracketScene.qml
        qml/FloatingText.qml
        qml/StatsScene.qml
        qml/GraphicsScene.qml
        qml/OptionsScene.qml
        qml/JoinScene.qml
        qml/LobbyScene.qml
        qml/PracticeLane.qml
        qml/ControlsScene.qml
        qml/OnlineScene.qml
        qml/PartyScene.qml
        qml/PhongMaterial.qml
        qml/Instances.qml
        qml/Discs.qml
        qml/Glow.qml
        qml/TextLayer.qml
        qml/TextMaterial.qml
        qml/Starfield.qml
        qml/MenuScene.qml
        qml/GameScene.qml
        qml/SettingsScene.qml
        qml/ResultScene.qml
    SOURCES
        src/player.h src/player.cpp
        src/match.h src/match.cpp
        src/partymatch.h src/partymatch.cpp
        src/partymodifiers.h src/partymodifiers.cpp
        src/highscores.h src/highscores.cpp
        src/onlineservice.h src/onlineservice.cpp
        src/fontatlas.h src/fontatlas.cpp
        src/textbatch.h src/textbatch.cpp
        src/dash.h src/dash.cpp
        src/computerplayer.h src/computerplayer.cpp
        src/tournament.h src/tournament.cpp
        src/modifiers.h src/modifiers.cpp
        src/arenas.h src/arenas.cpp
        src/soundeffects.h src/soundeffects.cpp
        src/music.h src/music.cpp
        src/stats.h src/stats.cpp
        src/graphicssettings.h src/graphicssettings.cpp
        src/gamesettings.h src/gamesettings.cpp
        src/keysettings.h src/keysettings.cpp
        src/gamepads.h src/gamepads.cpp
        src/lan.h src/lan.cpp
    RESOURCES
        config/modifiers.json
        config/arenas.json
        qml/phong.vert
        qml/phong_instanced.vert
        qml/phong.frag
        qml/glow_blur.frag
        qml/glow_combine.frag
        qml/text.vert
        qml/text.frag
)

# The generated QML type registration includes the headers by name
target_include_directories(phong PRIVATE src)

target_link_libraries(phong PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::Network
    Qt6::WebSockets
)

if(TARGET Qt6::Multimedia)
    target_link_libraries(phong PRIVATE Qt6::Multimedia)
    target_compile_definitions(phong PRIVATE PHONG_HAVE_MULTIMEDIA)
endif()

if(TARGET SDL3::SDL3)
    target_link_libraries(phong PRIVATE SDL3::SDL3)
    target_compile_definitions(phong PRIVATE PHONG_HAVE_SDL3)
    message(STATUS "Gamepads with SDL3 ${SDL3_VERSION}")
elseif(NOT EMSCRIPTEN)
    message(STATUS "No SDL3, no gamepads on the desktop")
endif()

set_target_properties(phong PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(EMSCRIPTEN)
    # PhysX and the Qt Quick 3D renderer need more than the default heap
    set_target_properties(phong PROPERTIES QT_WASM_INITIAL_MEMORY "128MB")
endif()

# Where the game asks which phong-server to use: a directory with
# { "server": "wss://..." }, online.json in the repository. Editing it
# moves the server or switches it off, without a build. See docs/online.md.
set(PHONG_DIRECTORY_URL "https://raw.githubusercontent.com/iam-peter/phong/main/online.json" CACHE STRING
    "Where the game asks for the server of games over the internet, empty for nowhere")
if(PHONG_DIRECTORY_URL)
    target_compile_definitions(phong PRIVATE PHONG_DIRECTORY_URL="${PHONG_DIRECTORY_URL}")
endif()
# The server until the directory answered once, e.g. wss://phong.example.com
set(PHONG_SERVER_URL "" CACHE STRING "The server for games over the internet before the directory answers")
if(PHONG_SERVER_URL)
    target_compile_definitions(phong PRIVATE PHONG_SERVER_URL="${PHONG_SERVER_URL}")
endif()

# The server for games over the internet, see server/
option(PHONG_BUILD_SERVER "Build the server for games over the internet" ON)
if(PHONG_BUILD_SERVER AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)
    add_subdirectory(server)
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
