#pragma once

#include "../ChatClient.h"
#include "Command.h"

class RoomsCommand : public Command {
  ChatClient *m_client;

public:
  explicit RoomsCommand(ChatClient *client)
      : Command("rooms", "List available rooms", QStringList{}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    m_client->sendGetRoomsRequest();
  }
};
