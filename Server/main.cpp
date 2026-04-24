#include "AdminConsole.h"

#include <QCommandLineParser>
#include <QCoreApplication>

#include "ChatServer.h"
#include "DatabaseManager.h"

#include <QSettings>
#include <iostream>

int main(int argc, char *argv[]) {
  const QCoreApplication a(argc, argv);
  QCoreApplication::setApplicationName("LanChatServer");
  QCoreApplication::setApplicationVersion("0.1");

  const QString configPath =
      QCoreApplication::applicationDirPath() + "/config.ini";
  const QSettings settings(configPath, QSettings::IniFormat);

  QCommandLineParser parser;
  parser.setApplicationDescription("Secure LAN Chat Server");
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption portOption(QStringList() << "p" << "port",
                                      "Server port.", "port", "3333");

  parser.addOption(portOption);

  parser.process(a);

  const QString port = parser.value(portOption);

  const QString dbPass = settings.value("Database/Password", "").toString();
  if (dbPass.isEmpty()) {
    std::cerr << "[Fatal Error] Database password is not set in config.ini!"
              << std::endl;
    return 1;
  }

  DatabaseManager dbManager;
  if (!dbManager.connectToDatabase(
          settings.value("Database/Host", "127.0.0.1").toString(),
          settings.value("Database/Name", "lanchat").toString(),
          settings.value("Database/User", "chatserver").toString(), dbPass)) {
    return 1;
  }

  ChatServer server(port.toUInt(), &dbManager);

  AdminConsole console(&dbManager);

  return QCoreApplication::exec();
}
