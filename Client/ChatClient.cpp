#include "ChatClient.h"

#include <print>

#include "Commands/ClientCommandRegistry.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QSslConfiguration>

#include <iostream>

ChatClient::ChatClient(QObject *parent) : QObject(parent), m_input(stdin) {
  m_socket = new QSslSocket(this);

  m_registry = std::make_unique<ClientCommandRegistry>(this);

  connect(m_socket, &QSslSocket::encrypted, this, &ChatClient::onConnected);
  connect(m_socket, &QSslSocket::readyRead, this, &ChatClient::onReadyRead);

  connect(m_socket, &QSslSocket::disconnected, this, []() {
    std::println("[System] Disconnected from server.");
    QCoreApplication::quit();
  });

  connect(m_socket, &QSslSocket::sslErrors, this,
          [this](const QList<QSslError> &errors) {
            for (const auto &error : errors)
              std::println("[SSL Warning] {}",
                           error.errorString().toStdString());

            m_socket->ignoreSslErrors();
          });

  connect(m_socket, &QAbstractSocket::errorOccurred, this,
          [this](QAbstractSocket::SocketError socketError) {
            if (socketError != QAbstractSocket::RemoteHostClosedError)
              std::println("[Network Error] {}",
                           m_socket->errorString().toStdString());
          });

  m_stdinNotifier = new QSocketNotifier(0, QSocketNotifier::Read, this);
  connect(m_stdinNotifier, &QSocketNotifier::activated, this,
          &ChatClient::onUserInput);
}

ChatClient::~ChatClient() = default;

void ChatClient::connectToServer(const QString &ip, quint16 port,
                                 const QString &login,
                                 const QString &password) {
  std::println("Connecting to {}:{}.", ip.toStdString(), port);

  m_login = login;
  m_password = password;

  m_socket->connectToHostEncrypted(ip, port);
}

void ChatClient::onConnected() {
  std::println("Connected to server!");

  if (!m_login.isEmpty() && !m_password.isEmpty()) {
    std::println("Attempting auto-login for user '{}'...",
                 m_login.toStdString());
    sendAuth(m_login, m_password);

    m_login.clear();
    m_password.clear();
  } else {
    std::println("Type '/help' to see available commands or '/login <login> "
                 "<pass>' to authenticate.");
  }
}

void ChatClient::onReadyRead() {
  const auto data = m_socket->readAll();

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    std::println("[System]: Error decrypting message");
    return;
  }

  const QJsonObject json = doc.object();
  const QString type = json["type"].toString();

  if (type == "auth_response") {
    handleAuthResponse(json);
  } else if (type == "message") {
    handleIncomingMessage(json);
  } else if (type == "system_message") {
    handleSystemMessage(json);
  } else if (type == "error") {
    std::println("[Server Error] {}", json["text"].toString().toStdString());
  } else {
    std::println("[Warning] Unknown packet type: {}", type.toStdString());
  }
}

void ChatClient::onUserInput() {
  const QString line = m_input.readLine().trimmed();

  if (line.isEmpty())
    return;

  if (line.startsWith("/")) {
    m_registry->processInput(line.mid(1));
  } else {
    if (m_isAuthorized) {
      sendChatMessage(line);
    } else {
      std::println("[Error] You must be logged in to send messages. Use "
                   "'/login <login> <pass>'.");
    }
  }
}

void ChatClient::sendAuth(const QString &login, const QString &password) const {
  const QJsonObject authMsg{
      {"type", "auth"}, {"login", login}, {"password", password}};

  m_socket->write(QJsonDocument(authMsg).toJson(QJsonDocument::Compact));
  m_socket->flush();
}

void ChatClient::sendChangePassword(const QString &newPassword,
                                    const QString &oldPassword) const {
  QJsonObject msg{{"type", "change_password"}, {"new_password", newPassword}};

  if (!oldPassword.isEmpty()) {
    msg["old_password"] = oldPassword;
  }

  m_socket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact));
  m_socket->flush();
}

void ChatClient::sendChatMessage(const QString &text) const {
  const QJsonObject message{{"type", "message"}, {"text", text}};

  m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact));
  m_socket->flush();
}

void ChatClient::handleAuthResponse(const QJsonObject &json) {
  const QString status = json["status"].toString();
  const QString text = json["text"].toString();

  if (status == "success") {
    m_isAuthorized = true;
    std::println("[System] {}", text.toStdString());
  } else if (status == "force_change") {
    std::println("[System] {}", text.toStdString());
    std::println(
        "[System] Please use '/changepass <new_password>' to continue.");
  }
}

void ChatClient::handleIncomingMessage(const QJsonObject &json) {
  std::println("[{}]: {}", json["nick"].toString().toStdString(),
               json["text"].toString().toStdString());
}

void ChatClient::handleSystemMessage(const QJsonObject &json) {
  std::println("[System] {}", json["text"].toString().toStdString());
}