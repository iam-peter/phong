# The server for games over the internet and the shared high scores. It
# only needs Qt Core and Qt WebSockets, 6.2 or newer, and builds on its
# own too, e.g. with the Qt of a distribution:
#   cmake -S server -B build/server && cmake --build build/server
cmake_minimum_required(VERSION 3.16)

if(NOT PROJECT_NAME)
    project(phong-server LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Core WebSockets)
set(CMAKE_AUTOMOC ON)

add_library(phongrelay STATIC
    relayserver.h relayserver.cpp
)
target_include_directories(phongrelay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(phongrelay PUBLIC Qt6::Core Qt6::WebSockets)

add_executable(phong-server
    main.cpp
)
target_link_libraries(phong-server PRIVATE phongrelay)

include(GNUInstallDirs)
install(TARGETS phong-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
