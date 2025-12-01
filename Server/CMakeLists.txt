add_executable(LanChatServer
        main.cpp
        ChatServer.cpp
        ChatServer.h
)

target_link_libraries(LanChatServer
        Qt::Core
        Qt::Network
)