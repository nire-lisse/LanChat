#include "ChatClient.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
  const QCoreApplication a(argc, argv);
  QCoreApplication::setApplicationName("LanChatClient");
  QCoreApplication::setApplicationVersion("0.1");

  QCommandLineParser parser;
  parser.setApplicationDescription("Secure LAN Chat Client");
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption ipOption(QStringList() << "i" << "ip",
                                    "Server IP address to connect.", "ip",
                                    "127.0.0.1");

  const QCommandLineOption portOption(QStringList() << "p" << "port",
                                      "Server port to connect.", "port",
                                      "3333");

  const QCommandLineOption loginOption(QStringList() << "l" << "login",
                                      "Your login.", "login", "");

  const QCommandLineOption passwordOption(QStringList() << "P" << "password",
                                     "Your password.", "password", "");

  parser.addOption(ipOption);
  parser.addOption(portOption);
  parser.addOption(loginOption);
  parser.addOption(passwordOption);

  parser.process(a);

  const QString ip = parser.value(ipOption);
  const QString port = parser.value(portOption);
  const QString login = parser.value(loginOption);
  const QString password = parser.value(passwordOption);

  ChatClient client;
  client.connectToServer(ip, port.toUInt(), login, password);

  return a.exec();
}
