#pragma once
#include <QProcess>
#include <QString>
#include <memory>

#include "../../Common/Command.h"
#include "ChangePassCommand.h"
#include "EditCommand.h"
#include "HelpCommand.h"
#include "HistoryCommand.h"
#include "LoginCommand.h"
#include "PinCommand.h"

class ChatClient;
class ClientCommandRegistry {
public:
  explicit ClientCommandRegistry(ChatClient *client) {
    m_rootCommand = std::make_shared<Command>("root", "");

    m_rootCommand->addSubCommand(
        std::make_shared<HelpCommand>(m_rootCommand.get()));
    m_rootCommand->addSubCommand(std::make_shared<LoginCommand>(client));
    m_rootCommand->addSubCommand(std::make_shared<ChangePassCommand>(client));

    m_rootCommand->addSubCommand(std::make_shared<EditCommand>(client));
    m_rootCommand->addSubCommand(std::make_shared<PinCommand>(client, true));
    m_rootCommand->addSubCommand(std::make_shared<PinCommand>(client, false));
    m_rootCommand->addSubCommand(std::make_shared<HistoryCommand>(client));
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