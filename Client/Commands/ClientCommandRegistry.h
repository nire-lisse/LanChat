#pragma once
#include <QProcess>
#include <QString>
#include <memory>

#include "../../Common/Command.h"
#include "ChangePassCommand.h"
#include "HelpCommand.h"
#include "LoginCommand.h"

class ChatClient;
class ClientCommandRegistry {
public:
  explicit ClientCommandRegistry(ChatClient *client) {
    m_rootCommand = std::make_shared<Command>("root", "");

    m_rootCommand->addSubCommand(
        std::make_shared<HelpCommand>(m_rootCommand.get()));
    m_rootCommand->addSubCommand(std::make_shared<LoginCommand>(client));
    m_rootCommand->addSubCommand(std::make_shared<ChangePassCommand>(client));
  }

  void processInput(const QString &line) const {
    const QStringList args = QProcess::splitCommand(line);
    if (args.isEmpty())
      return;

    m_rootCommand->execute(args);
  }

private:
  std::shared_ptr<Command> m_rootCommand;
};