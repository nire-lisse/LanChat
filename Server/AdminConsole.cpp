#include "AdminConsole.h"
#include <print>

AdminConsole::AdminConsole(DatabaseManager *dbManager, QObject *parent)
    : QObject(parent), m_input(stdin) {
  m_registry = std::make_unique<ServerCommandRegistry>(dbManager);

  m_notifier = new QSocketNotifier(0, QSocketNotifier::Read, this);
  connect(m_notifier, &QSocketNotifier::activated, this,
          &AdminConsole::onUserInput);

  std::println("Admin console ready. Type 'help' for commands.");
}

AdminConsole::~AdminConsole() = default;

void AdminConsole::onUserInput() {
  const QString line = m_input.readLine().trimmed();
  if (line.isEmpty())
    return;

  m_registry->processInput(line);
}
