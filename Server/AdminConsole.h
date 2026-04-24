#pragma once

#include "Commands/ServerCommandRegistry.h"
#include "DatabaseManager.h"
#include <QObject>
#include <QSocketNotifier>
#include <QTextStream>

class AdminConsole : public QObject {
  Q_OBJECT
public:
  explicit AdminConsole(DatabaseManager *dbManager, QObject *parent = nullptr);
  ~AdminConsole() override;

private slots:
  void onUserInput();

private:
  QSocketNotifier *m_notifier;
  QTextStream m_input;
  std::unique_ptr<ServerCommandRegistry> m_registry;
};