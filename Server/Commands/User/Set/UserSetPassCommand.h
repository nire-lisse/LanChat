#pragma once
#include "../../../DatabaseManager.h"
#include "Command.h"
#include <print>

class UserSetPassCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserSetPassCommand(DatabaseManager *db)
      : Command("pass", "Reset user password (force change on next login)",
                QStringList{"<login>", "<new_password>"}),
        m_db(db) {}

  void execute(const QStringList &args) override {
    const QString login = args[0];

    if (QString newPass = args[1]; m_db->setPassword(login, newPass, false))
      std::println("[Success] Password for '{}' has been reset.",
                   login.toStdString());
    else
      std::println("[Error] Could not reset password for '{}'.",
                   login.toStdString());
  }
};