#include "ChatClient.h"

#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char *argv[]) {
    const QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("LanChatClient");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Secure LAN Chat Client");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption ipOption(
        QStringList() << "i" << "ip",
        "Server IP address to connect.",
        "ip",
        "127.0.0.1");

    const QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "Server port to connect.",
        "port",
        "3333");

    const QCommandLineOption nickOption(
        QStringList() << "n" << "nickname",
        "Your nickname.",
        "nickname",
        "");

    const QCommandLineOption keyOption(
        QStringList() << "k" << "key",
        "Crypto key.",
        "key",
        "");

    parser.addOption(ipOption);
    parser.addOption(portOption);
    parser.addOption(nickOption);
    parser.addOption(keyOption);

    parser.process(a);

    const QString ip = parser.value(ipOption);
    const QString port = parser.value(portOption);
    const QString nick = parser.value(nickOption);
    const QString key = parser.value(keyOption);

    ChatClient client;
    client.connectToServer(ip, port.toUInt(), nick, key);

    return a.exec();
}
