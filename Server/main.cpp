#include <QCommandLineParser>
#include <QCoreApplication>
#include "ChatServer.h"

int main(int argc, char *argv[]) {
    const QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("LanChatServer");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Secure LAN Chat Server");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "Server port.",
        "port",
        "3333");

    parser.addOption(portOption);

    parser.process(a);

    const QString port = parser.value(portOption);

    ChatServer server(port.toUInt());

    return QCoreApplication::exec();
}
