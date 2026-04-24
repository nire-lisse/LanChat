#include "Command.h"

#include <QTextStream>
#include <print>
#include <utility>

Command::Command(QString name, QString description, QStringList args)
    : m_name(std::move(name)), m_description(std::move(description)),
      m_args(std::move(args)) {}

void Command::execute(const QStringList &args) {
  if (args.isEmpty()) {
    std::println(
        "[Error] Missing sub-command for {}. Type 'help' for more information.",
        m_name.toStdString());
    return;
  }

  const QString subName = args[0].toLower();
  if (!m_subCommands.contains(subName)) {
    std::println("[Error] Unknown sub-command '{}'. Type 'help' for more "
                 "information.",
                 subName.toStdString());
    return;
  }

  const auto subCmd = m_subCommands[subName];
  const QStringList subArgs = args.mid(1);

  if (subArgs.size() < subCmd->getRequiredArgsCount()) {
    std::println("[Error] Usage: {} {} {}", m_name.toStdString(),
                 subName.toStdString(), subCmd->m_args.join(" ").toStdString());
    return;
  }

  subCmd->execute(subArgs);
}

void Command::addSubCommand(const std::shared_ptr<Command> &cmd) {
  m_subCommands[cmd->getName()] = cmd;
}

QString Command::help(const QString &path) const {
  QString currentPath;

  if (m_name != "root") {
    currentPath = path.isEmpty() ? m_name : path + " " + m_name;
  }

  const QString title = currentPath.isEmpty() ? "Main Menu" : currentPath;

  QString out;
  QTextStream stream(&out);

  stream << "--- Help: " << title << " ---\n";

  if (!m_description.isEmpty())
    stream << "Description: " << m_description << "\n";

  stream << "Usage:";
  if (!currentPath.isEmpty())
    stream << " " << currentPath;

  if (QString usageStr = getUsageString(); !usageStr.isEmpty())
    stream << " " << usageStr;

  stream << "\n";

  if (!m_subCommands.isEmpty()) {
    stream << "\nSub-commands:\n";
    for (auto it = m_subCommands.begin(); it != m_subCommands.end(); ++it) {
      QString nameAndUsage = it.key();

      if (QString subUsage = it.value()->getUsageString(); !subUsage.isEmpty())
        nameAndUsage += " " + subUsage;

      stream << "  " << nameAndUsage.leftJustified(10, ' ') << " - "
             << it.value()->getDescription() << "\n";
    }
  }

  stream << "-----------------";

  return out;
}

QString Command::getName() const { return m_name; }

QString Command::getDescription() const { return m_description; }

std::shared_ptr<Command> Command::getSubCommand(const QString &name) const {
  return m_subCommands.value(name, nullptr);
}

QMap<QString, std::shared_ptr<Command>> Command::getAllSubCommands() const {
  return m_subCommands;
}

int Command::getRequiredArgsCount() const {
  int count = 0;
  for (const auto &arg : m_args)
    if (!arg.startsWith("["))
      count++;

  return count;
}

QString Command::getUsageString() const {
  QStringList parts;

  if (!m_subCommands.isEmpty())
    parts << "<" + m_subCommands.keys().join("|") + ">";

  if (!m_args.isEmpty())
    parts << m_args.join(" ");

  return parts.join(" ");
}