find_package(Qt6 REQUIRED COMPONENTS Test)

function(phong_add_test name)
    qt_add_executable(${name} ${name}.cpp ${ARGN})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

phong_add_test(tst_match
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

phong_add_test(tst_modifiers
    ${PROJECT_SOURCE_DIR}/src/modifiers.cpp
    ${PROJECT_SOURCE_DIR}/src/match.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
)

phong_add_test(tst_computerplayer
    ${PROJECT_SOURCE_DIR}/src/computerplayer.cpp
)
