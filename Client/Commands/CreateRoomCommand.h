#pragma once

#include "../ChatClient.h"
#include "Command.h"
#include <print>

class CreateRoomCommand : public Command {
  ChatClient *m_client;

public:
  explicit CreateRoomCommand(ChatClient *client)
      : Command("createroom",
                "Create a new room (use quotes for multi-word description)",
                QStringList{"<name>", "<type: 0=Pub, 1=Priv>", "[description]"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    const QString name = args[0];

    bool ok;
    const int type = args[1].toInt(&ok);
    if (!ok || (type != 0 && type != 1)) {
      std::println("[Error] Room type must be 0 (Public) or 1 (Private).");
      return;
    }

    const QString description = (args.size() > 2) ? args[2] : "";

    m_client->sendCreateRoomRequest(name, type, description);
    std::println("[System] Requesting creation of room '{}'...", name.toStdString());
  }
};