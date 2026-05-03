#pragma once
#include "../ChatClient.h"
#include "Command.h"
#include <print>

class PinCommand : public Command {
  ChatClient *m_client;
  bool m_isPinning;

public:
  explicit PinCommand(ChatClient *client, const bool isPinning = true)
      : Command(isPinning ? "pin" : "unpin",
                isPinning ? "Pin a message" : "Unpin a message",
                QStringList{"<message_id>"}),
        m_client(client), m_isPinning(isPinning) {}

  void execute(const QStringList &args) override {
    bool ok;
    const qint64 msgId = args[0].toLongLong(&ok);
    if (!ok) {
      std::println("[Error] Invalid message ID.");
      return;
    }

    m_client->sendPinMessage(msgId, m_isPinning);
  }
};