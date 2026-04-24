#pragma once
#include <QProcess>
#include <QString>
#include <memory>

#include "../Common/Command.h"
#include "HelpCommand.h"
#include "UserCommand.h"

class ServerCommandRegistry {
public:
  explicit ServerCommandRegistry(DatabaseManager *dbManager) {
    m_rootCommand = std::make_shared<Command>("root", "");

    m_rootCommand->addSubCommand(
        std::make_shared<HelpCommand>(m_rootCommand.get()));

    m_rootCommand->addSubCommand(std::make_shared<UserCommand>(dbManager));
  }

  void processInput(const QString &line) const {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
      return;

    const QStringList args = QProcess::splitCommand(trimmed);
    if (args.isEmpty())
      return;

    m_rootCommand->execute(args);
  }

private:
  std::shared_ptr<Command> m_rootCommand;
};