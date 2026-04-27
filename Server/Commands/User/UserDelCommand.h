#pragma once
#include "../../DatabaseManager.h"
#include "Command.h"
#include "spdlog/spdlog.h"

class UserDelCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserDelCommand(DatabaseManager *db)
      : Command("del", "Delete a user", QStringList{"<login>"}), m_db(db) {}

  void execute(const QStringList &args) override {
    if (const QString login = args[0]; m_db->deleteUser(login))
      spdlog::info("User '{}' marked as deleted.", login.toStdString());
    else
      spdlog::warn("Failed to delete user '{}'. Ensure login is "
                   "correct and user is not already deleted.",
                   login.toStdString());
  }
};