add_executable(LanChatClient
        main.cpp
        ChatClient.h
        ChatClient.cpp
)

target_link_libraries(LanChatClient
        Qt::Core
        Qt::Network
        LanChatCommon
)