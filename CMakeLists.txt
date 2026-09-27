cmake_minimum_required(VERSION 3.16)
project(chess_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

add_executable(chess_console
    src/main.cpp
)

target_include_directories(chess_console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(CTest)
if(BUILD_TESTING)
    add_executable(network_protocol_test
        tests/network_protocol_test.cpp
    )
    add_test(NAME network_protocol_test COMMAND network_protocol_test)
endif()

add_executable(chess_gui
    gui/main.cpp
    gui/MainWindow.cpp
    gui/GameSession.cpp
    gui/NetworkSession.cpp
    gui/BoardWidget.cpp
    gui/StartPage.cpp
    gui/LobbyPage.cpp
    gui/ModeSelectPage.cpp
    gui/WaitingRoomPage.cpp
    gui/GamePage.cpp
)

target_include_directories(chess_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(chess_gui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_link_libraries(chess_gui PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

add_executable(relay_server
    relay_server/main.cpp
    relay_server/RelayServer.cpp
)

target_include_directories(relay_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/relay_server
)

set_target_properties(relay_server PROPERTIES
    AUTOMOC ON
)

target_link_libraries(relay_server PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32 AND MINGW)
    target_link_options(chess_console PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(chess_gui PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(relay_server PRIVATE -static-libgcc -static-libstdc++)
endif()
