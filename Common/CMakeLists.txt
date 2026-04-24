add_library(LanChatCommon STATIC
        CryptoHelper.h
        CryptoHelper.cpp
        Command.cpp
        Command.h
)

target_link_libraries(LanChatCommon PUBLIC Qt::Core)
target_include_directories(LanChatCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
