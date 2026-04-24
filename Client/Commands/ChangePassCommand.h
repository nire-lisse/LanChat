#pragma once
#include "../ChatClient.h"
#include "Command.h"

#include <print>

class ChangePassCommand : public Command {
  ChatClient *m_client;

public:
  explicit ChangePassCommand(ChatClient *client)
      : Command("changepass", "Change your password",
                QStringList{"<new_password>", "[old_password]"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    const QString newPass = args[0];
    const QString oldPass = args.size() > 1 ? args[1] : "";

    m_client->sendChangePassword(newPass, oldPass);
    std::println("Requesting password change...");
  }
};
