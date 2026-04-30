add_library(LanChatCommon STATIC
        Command.cpp
        Command.h
)

include(FetchContent)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
)

FetchContent_MakeAvailable(spdlog)

target_link_libraries(LanChatCommon PUBLIC
        Qt::Core
        spdlog::spdlog
)

target_include_directories(LanChatCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
