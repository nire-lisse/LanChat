add_library(LanChatCommon STATIC
        CryptoHelper.h
        CryptoHelper.cpp
)

target_link_libraries(LanChatCommon PUBLIC Qt::Core)
target_include_directories(LanChatCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
