#include "ChatClient.h"
#include "CryptoHelper.h"

#include <QJsonObject>

#include <print>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent), m_input(stdin) {
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);

    m_stdinNotifier = new QSocketNotifier(0, QSocketNotifier::Read, this);
    connect(m_stdinNotifier, &QSocketNotifier::activated, this, &ChatClient::onUserInput);
}

void ChatClient::connectToServer(const QString &ip, quint16 port, const QString &nick, const QString &k) {
    std::println("Connecting to {}:{}.", ip.toStdString(), port);

    if (nick.isEmpty()) {
        std::print("Enter nickname: ");
        this->nickname = m_input.readLine();
    } else {
        this->nickname = nick;
    }

    if (k.isEmpty()) {
        std::print("Enter key: ");
        this->key = m_input.readLine();
    } else {
        this->key = k;
    }

    m_socket->connectToHost(ip, port);
}

void ChatClient::onConnected() {
    std::println("Connected! Type message and press Enter:");
}

void ChatClient::onReadyRead() {
    const auto data = m_socket->readAll();

    const auto decryptData = CryptoHelper::process(data, key);

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(decryptData, &error);

    if (error.error != QJsonParseError::NoError) {
        std::println("[System]: Error decrypting message");
        return;
    }

    QJsonObject message = doc.object();

    const auto nick = message["nick"].toString();
    const auto text = message["text"].toString();

    std::println("[{}]: {}", nick.toStdString(), text.toStdString());
}

void ChatClient::onUserInput() {
    const QString line = m_input.readLine();

    if (line.isEmpty()) {
        return;
    }

    QJsonObject message;
    message.insert("nick", nickname);
    message.insert("text", line);

    const QByteArray encryptedBytes = CryptoHelper::process(QJsonDocument(message).toJson(), key);

    m_socket->write(encryptedBytes);
    m_socket->flush();
}
