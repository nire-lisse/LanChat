#pragma once

#include "../ChatClient.h"
#include "Command.h"

class JoinCommand : public Command {
  ChatClient *m_client;

public:
  explicit JoinCommand(ChatClient *client)
      : Command("join",
                "Join a room (e.g. /join Global) or direct message (e.g. /join "
                "@alice)",
                QStringList{"<name>"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    m_client->sendJoinRoomRequest(args[0]);
  }
};
