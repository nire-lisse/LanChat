#pragma once
#include "../ChatClient.h"
#include "Command.h"
#include <print>

class EditCommand : public Command {
  ChatClient *m_client;

public:
  explicit EditCommand(ChatClient *client)
      : Command("edit", "Edit a specific message",
                QStringList{"<message_id>", "<new_text...>"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    bool ok;
    const qint64 msgId = args[0].toLongLong(&ok);
    if (!ok) {
      std::println("[Error] Invalid message ID.");
      return;
    }

    const QString newText = args.mid(1).join(" ");

    m_client->sendEditMessage(msgId, newText);
  }
};