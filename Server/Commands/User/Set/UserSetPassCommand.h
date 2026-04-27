#pragma once
#include "../../../DatabaseManager.h"
#include "Command.h"

class UserSetPassCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserSetPassCommand(DatabaseManager *db)
      : Command("pass", "Reset user password (force change on next login)",
                QStringList{"<login>", "<new_password>"}),
        m_db(db) {}

  void execute(const QStringList &args) override {
    const QString login = args[0];

    if (const QString newPass = args[1];
        m_db->setPassword(login, newPass, false))
      spdlog::info("Password for '{}' has been reset.", login.toStdString());
    else
      spdlog::warn("Could not reset password for '{}'.", login.toStdString());
  }
};