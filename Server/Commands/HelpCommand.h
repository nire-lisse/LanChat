#pragma once
#include "Command.h"
#include <print>

class HelpCommand : public Command {
  Command *m_rootCommand;

public:
  explicit HelpCommand(Command *rootNode)
      : Command("help", "Show available commands", QStringList{"[command]"}),
        m_rootCommand(rootNode) {}

  void execute(const QStringList &args) override {
    const Command *current = m_rootCommand;
    QStringList currentPath;

    for (const QString &arg : args) {
      QString target = arg.toLower();
      auto nextCmd = current->getSubCommand(target);

      if (!nextCmd) {
        const QString fullPath = currentPath.isEmpty()
                                     ? target
                                     : currentPath.join(" ") + " " + target;
        std::println("[Error] No help available for unknown command '{}'.",
                     fullPath.toStdString());
        return;
      }

      current = nextCmd.get();
      currentPath.append(current->getName());
    }

    QString pathPrefix;
    if (!currentPath.isEmpty()) {
      currentPath.removeLast();
      pathPrefix = currentPath.join(" ");
    }

    std::println("{}", current->help(pathPrefix).toStdString());
  }
};