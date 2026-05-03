#include "AdminConsole.h"

#include <QCommandLineParser>
#include <QCoreApplication>

#include "ChatServer.h"
#include "Database/DatabaseManager.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks-inl.h"
#include "spdlog/spdlog.h"

#include <QSettings>

void setupServerLogger() {
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_pattern("[%d/%m/%Y %H:%M:%S %z] [%^%l%$] %v");

  auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
      "logs/daily.txt", 0, 0);
  file_sink->set_pattern("[%H:%M:%S %z] [%^%l%$] %v");

  auto logger = std::make_shared<spdlog::logger>(
      "server_logger", spdlog::sinks_init_list{console_sink, file_sink});

  spdlog::set_default_logger(logger);
  spdlog::flush_every(std::chrono::seconds(1));
}

int main(int argc, char *argv[]) {
  const QCoreApplication a(argc, argv);
  QCoreApplication::setApplicationName("LanChatServer");
  QCoreApplication::setApplicationVersion("0.1");

  setupServerLogger();

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
  const QString dbKey = settings.value("Database/EncryptionKey", "").toString();

  if (dbPass.isEmpty() || dbKey.isEmpty()) {
    spdlog::error(
        "Database password or encryption key is not set in config.ini!");

    return 1;
  }

  DatabaseManager dbManager;
  if (!dbManager.connectToDatabase(
          settings.value("Database/Host", "127.0.0.1").toString(),
          settings.value("Database/Name", "lanchat").toString(),
          settings.value("Database/User", "chatserver").toString(), dbPass,
          dbKey)) {
    return 1;
  }

  ChatServer server(port.toUInt(), &dbManager);

  AdminConsole console(&dbManager);

  return QCoreApplication::exec();
}
