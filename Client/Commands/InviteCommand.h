#pragma once

#include "../ChatClient.h"
#include "Command.h"
#include <print>

class InviteCommand : public Command {
  ChatClient *m_client;

public:
  explicit InviteCommand(ChatClient *client)
      : Command("invite", "Invite a user to the current room", QStringList{"<login>"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    m_client->sendInviteRequest(args[0]);
    std::println("[System] Sending invite for '{}'...", args[0].toStdString());
  }
};