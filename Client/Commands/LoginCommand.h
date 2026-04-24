#pragma once
#include "../ChatClient.h"
#include "Command.h"
#include <print>

class LoginCommand : public Command {
  ChatClient *m_client;

public:
  explicit LoginCommand(ChatClient *client)
      : Command("login", "Authenticate with the server",
                QStringList{"<login>", "<password>"}),
        m_client(client) {}

  void execute(const QStringList &args) override {
    const QString login = args[0];
    const QString password = args[1];

    m_client->sendAuth(login, password);

    std::println("[System] Sending authentication request for '{}'...",
                 login.toStdString());
  }
};