#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>

class Command {
public:
  Command(QString name, QString description, QStringList args = QStringList());
  virtual ~Command() = default;

  virtual void execute(const QStringList &args);

  void addSubCommand(const std::shared_ptr<Command> &cmd);

  [[nodiscard]] virtual QString help(const QString &path) const;

  [[nodiscard]] QString getName() const;
  [[nodiscard]] QString getDescription() const;
  [[nodiscard]] std::shared_ptr<Command>
  getSubCommand(const QString &name) const;
  [[nodiscard]] QMap<QString, std::shared_ptr<Command>>
  getAllSubCommands() const;

protected:
  QString m_name;
  QString m_description;
  QStringList m_args;

  QMap<QString, std::shared_ptr<Command>> m_subCommands;

  [[nodiscard]] int getRequiredArgsCount() const;

  [[nodiscard]] QString getUsageString() const;
};