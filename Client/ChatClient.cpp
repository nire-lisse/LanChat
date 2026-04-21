#include "ChatClient.h"

#include <print>

#include "CryptoHelper.h"

#include <QCoreApplication>
#include <QJsonObject>

ChatClient::ChatClient(QObject *parent) : QObject(parent), m_input(stdin) {
  m_socket = new QTcpSocket(this);

  connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
  connect(m_socket, &QTcpSocket::disconnected, this, []() {
    std::println("Disconnected from server. (Wrong password or server down)");
    QCoreApplication::quit();
  });

  m_stdinNotifier = new QSocketNotifier(0, QSocketNotifier::Read, this);
  connect(m_stdinNotifier, &QSocketNotifier::activated, this,
          &ChatClient::onUserInput);
}

void ChatClient::connectToServer(const QString &ip, quint16 port,
                                 const QString &login,
                                 const QString &password) {
  std::println("Connecting to {}:{}.", ip.toStdString(), port);

  if (login.isEmpty()) {
    std::print("Enter login: ");
    m_login = m_input.readLine();
  } else {
    m_login = login;
  }

  if (password.isEmpty()) {
    std::print("Enter password: ");
    m_password = m_input.readLine();
  } else {
    m_password = password;
  }

  m_socket->connectToHost(ip, port);
}

void ChatClient::onConnected() {
  std::println("Connected to server! Sending auth request...");

  QJsonObject authMsg;
  authMsg["type"] = "auth";
  authMsg["login"] = m_login;
  authMsg["password"] = m_password;

  const QByteArray authData =
      QJsonDocument(authMsg).toJson(QJsonDocument::Compact);
  m_socket->write(CryptoHelper::autoProcess(authData));
  m_socket->flush();

  std::println("Auth sent.");
}

void ChatClient::onReadyRead() {
  const auto data = m_socket->readAll();

  const auto decryptData = CryptoHelper::autoProcess(data);

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
  message["type"] = "message";
  message["text"] = line;

  const QByteArray encryptedBytes =
      CryptoHelper::autoProcess(QJsonDocument(message).toJson());

  m_socket->write(encryptedBytes);
  m_socket->flush();
}
