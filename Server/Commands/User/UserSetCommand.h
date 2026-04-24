#pragma once
#include "Command.h"
#include "Set/UserSetPassCommand.h"

class UserSetCommand : public Command {
public:
  explicit UserSetCommand(DatabaseManager *db)
      : Command("set", "Modify user properties") {
    addSubCommand(std::make_shared<UserSetPassCommand>(db));
  }
};