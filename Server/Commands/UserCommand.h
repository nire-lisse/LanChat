#pragma once
#include "Command.h"
#include "User/UserAddCommand.h"
#include "User/UserDelCommand.h"
#include "User/UserListCommand.h"
#include "User/UserSetCommand.h"

class UserCommand : public Command {
public:
  explicit UserCommand(DatabaseManager *dbManager)
      : Command("user", "User management commands") {
    addSubCommand(std::make_shared<UserAddCommand>(dbManager));
    addSubCommand(std::make_shared<UserDelCommand>(dbManager));
    addSubCommand(std::make_shared<UserListCommand>(dbManager));
    addSubCommand(std::make_shared<UserSetCommand>(dbManager));
  }
};
