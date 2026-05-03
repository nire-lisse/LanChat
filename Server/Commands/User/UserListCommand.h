#pragma once
#include "../../Database/DatabaseManager.h"
#include "Command.h"

#include <print>

class UserListCommand : public Command {
  DatabaseManager *m_db;

public:
  explicit UserListCommand(DatabaseManager *db)
      : Command("list", "List all users"), m_db(db) {}

  void execute(const QStringList & /*args*/) override {
    auto users = m_db->users().getUsers();

    if (users.isEmpty()) {
      std::println("No users found.");
      return;
    }

    std::println("{:<5} | {:<15} | {:<20}", "ID", "Login", "Nickname");
    std::println("{:-<42}", "");

    for (const auto &[id, login, nickname] : users)
      std::println("{:<5} | {:<15} | {:<20}", id, login.toStdString(),
                   nickname.toStdString());
  }
};