#pragma once
#include "../ChatClient.h"
#include "Command.h"
#include <print>

class HistoryCommand : public Command {
  ChatClient *m_client;

public:
  explicit HistoryCommand(ChatClient *client)
      : Command("history", "Load older messages", QStringList{"[before_id]"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    qint64 beforeId = 0;

    bool ok;
    beforeId = args[0].toLongLong(&ok);
    if (!ok) {
      std::println("[Error] Invalid message ID.");
      return;
    }

    m_client->sendHistoryRequest(beforeId);
  }
};