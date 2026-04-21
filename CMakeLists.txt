cmake_minimum_required(VERSION 4.1)
project(LanChatConsole)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Network
        Sql
        REQUIRED)

add_subdirectory(Common)
add_subdirectory(Client)
add_subdirectory(Server)

