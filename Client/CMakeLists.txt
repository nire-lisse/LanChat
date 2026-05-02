add_executable(LanChatClient
        main.cpp
        ChatClient.h
        ChatClient.cpp
        Commands/HelpCommand.h
        Commands/ClientCommandRegistry.h
        Commands/ChangePassCommand.h
        Commands/LoginCommand.h
        Commands/EditCommand.h
        Commands/PinCommand.h
        Commands/HistoryCommand.h
)

target_link_libraries(LanChatClient
        Qt::Core
        Qt::Network
        LanChatCommon
)