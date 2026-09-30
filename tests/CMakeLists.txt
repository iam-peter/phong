find_package(Qt6 REQUIRED COMPONENTS Test Network WebSockets)

function(phong_add_test name)
    qt_add_executable(${name} ${name}.cpp ${ARGN})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(${name} PRIVATE PHONG_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # Without a console Qt on Windows logs to the debugger, the test output
    # would be missing from ctest
    if(WIN32)
        set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT QT_FORCE_STDERR_LOGGING=1)
    endif()
endfunction()

phong_add_test(tst_match
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

phong_add_test(tst_partymatch
    ${PROJECT_SOURCE_DIR}/src/partymatch.cpp
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

phong_add_test(tst_modifiers
    ${PROJECT_SOURCE_DIR}/src/modifiers.cpp
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

phong_add_test(tst_partymodifiers
    ${PROJECT_SOURCE_DIR}/src/partymodifiers.cpp
    ${PROJECT_SOURCE_DIR}/src/partymatch.cpp
    ${PROJECT_SOURCE_DIR}/src/modifiers.cpp
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Quick3D)
phong_add_test(tst_fontatlas
    ${PROJECT_SOURCE_DIR}/src/fontatlas.cpp
)
target_link_libraries(tst_fontatlas PRIVATE Qt6::Quick3D)
set_property(TEST tst_fontatlas APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
# On Windows the offscreen platform only looks for fonts in Qt's lib/fonts,
# which Qt doesn't ship
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{WINDIR}/Fonts" windows_fonts)
    set_property(TEST tst_fontatlas APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=${windows_fonts}")
endif()

phong_add_test(tst_onlineservice
    ${PROJECT_SOURCE_DIR}/src/onlineservice.cpp
    ${PROJECT_SOURCE_DIR}/src/lan.cpp
)
target_link_libraries(tst_onlineservice PRIVATE Qt6::Network Qt6::WebSockets)
if(TARGET phongrelay)
    target_link_libraries(tst_onlineservice PRIVATE phongrelay)
    target_compile_definitions(tst_onlineservice PRIVATE PHONG_HAVE_RELAY)
endif()

phong_add_test(tst_dash
    ${PROJECT_SOURCE_DIR}/src/dash.cpp
)

phong_add_test(tst_computerplayer
    ${PROJECT_SOURCE_DIR}/src/computerplayer.cpp
)

phong_add_test(tst_tournament
    ${PROJECT_SOURCE_DIR}/src/tournament.cpp
    ${PROJECT_SOURCE_DIR}/src/computerplayer.cpp
)

phong_add_test(tst_arenas
    ${PROJECT_SOURCE_DIR}/src/arenas.cpp
)

phong_add_test(tst_stats
    ${PROJECT_SOURCE_DIR}/src/stats.cpp
)

phong_add_test(tst_keysettings
    ${PROJECT_SOURCE_DIR}/src/keysettings.cpp
)

phong_add_test(tst_gamepads
    ${PROJECT_SOURCE_DIR}/src/gamepads.cpp
)
if(TARGET SDL3::SDL3)
    target_link_libraries(tst_gamepads PRIVATE SDL3::SDL3)
    target_compile_definitions(tst_gamepads PRIVATE PHONG_HAVE_SDL3)
endif()

phong_add_test(tst_lan
    ${PROJECT_SOURCE_DIR}/src/lan.cpp
)
target_link_libraries(tst_lan PRIVATE Qt6::Network Qt6::WebSockets)
if(TARGET phongrelay)
    target_link_libraries(tst_lan PRIVATE phongrelay)
    target_compile_definitions(tst_lan PRIVATE PHONG_HAVE_RELAY)
endif()

if(TARGET phongrelay)
    phong_add_test(tst_highscores
        ${PROJECT_SOURCE_DIR}/src/highscores.cpp
        ${PROJECT_SOURCE_DIR}/src/lan.cpp
    )
    target_link_libraries(tst_highscores PRIVATE phongrelay Qt6::Network Qt6::WebSockets)

    phong_add_test(tst_server)
    target_link_libraries(tst_server PRIVATE phongrelay Qt6::WebSockets)
endif()

phong_add_test(tst_graphicssettings
    ${PROJECT_SOURCE_DIR}/src/graphicssettings.cpp
)

phong_add_test(tst_gamesettings
    ${PROJECT_SOURCE_DIR}/src/gamesettings.cpp
    ${PROJECT_SOURCE_DIR}/src/computerplayer.cpp
)

# Without an audio backend, the synthesis is what's tested
phong_add_test(tst_soundeffects
    ${PROJECT_SOURCE_DIR}/src/soundeffects.cpp
    ${PROJECT_SOURCE_DIR}/src/music.cpp
)

# The built-in configuration, at the path the app has it
qt_add_resources(tst_modifiers "modifiers_config"
    PREFIX "/qt/qml/Phong"
    BASE ${PROJECT_SOURCE_DIR}
    FILES ${PROJECT_SOURCE_DIR}/config/modifiers.json
)

qt_add_resources(tst_partymodifiers "partymodifiers_config"
    PREFIX "/qt/qml/Phong"
    BASE ${PROJECT_SOURCE_DIR}
    FILES ${PROJECT_SOURCE_DIR}/config/modifiers.json
)

qt_add_resources(tst_arenas "arenas_config"
    PREFIX "/qt/qml/Phong"
    BASE ${PROJECT_SOURCE_DIR}
    FILES ${PROJECT_SOURCE_DIR}/config/arenas.json
)
