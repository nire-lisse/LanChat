#pragma once
#include "../../DatabaseManager.h"
#include "Command.h"

class UserAddCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserAddCommand(DatabaseManager *dbManager)
      : Command("add", "Create a new user",
                QStringList{"<login>", "<password>", "<nickname>"}),
        m_db(dbManager) {}

  void execute(const QStringList &args) override {
    if (m_db->addUser(args[0], args[1], args[2])) {
      std::println("[Success] User '{}' added.", args[0].toStdString());
    } else {
      std::println("[Error] Failed to add user.");
    }
  }
};
