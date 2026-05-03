#pragma once
#include "../../../Database/DatabaseManager.h"
#include "Command.h"

class UserSetNicknameCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserSetNicknameCommand(DatabaseManager *db)
      : Command("nick", "Set user nickname",
                QStringList{"<login>", "<new_nickname>"}),
        m_db(db) {}

  void execute(const QStringList &args) override {
    const QString login = args[0];

    if (const QString newNick = args[1];
        m_db->users().setNickname(login, newNick))
      spdlog::info("Nickname for '{}' has been set.", login.toStdString());
    else
      spdlog::warn("Could not reset nickname for '{}'.", login.toStdString());
  }
};