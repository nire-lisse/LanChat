#include "ChatServer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QSettings>

#include "CryptoHelper.h"
#include "DatabaseManager.h"
#include "spdlog/spdlog.h"

ChatServer::ChatServer(const quint16 port, DatabaseManager *db, QObject *parent)
    : QObject(parent) {
  m_dbManager = db;

  m_server = new QTcpServer(this);
  connect(m_server, &QTcpServer::newConnection, this,
          &ChatServer::onNewConnection);

  if (m_server->listen(QHostAddress::Any, port))
    spdlog::info("Server started on port {}", port);
  else
    spdlog::error("Server failed: {}", m_server->errorString().toStdString());
}

ChatServer::~ChatServer() {
  for (QTcpSocket *socket : m_clients) {
    socket->close();
    socket->deleteLater();
  }
  m_server->close();
}

void ChatServer::onNewConnection() {
  QTcpSocket *clientSocket = m_server->nextPendingConnection();
  m_clients.append(clientSocket);

  const auto clientAddress = clientSocket->peerAddress();

  spdlog::info(">>> New connection from: {}",
               clientAddress.toString().toStdString());

  connect(clientSocket, &QTcpSocket::readyRead, this,
          &ChatServer::onClientReadyRead);
  connect(clientSocket, &QTcpSocket::disconnected, this,
          &ChatServer::onClientDisconnected);
}

void ChatServer::onClientReadyRead() {
  const auto senderSocket = qobject_cast<QTcpSocket *>(sender());
  if (!senderSocket)
    return;

  const QByteArray data = senderSocket->readAll();
  const QByteArray decryptedData = CryptoHelper::autoProcess(data);

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(decryptedData, &error);

  if (error.error != QJsonParseError::NoError)
    return;

  const QJsonObject json = doc.object();
  const QString type = json["type"].toString();

  bool isAuthorized = senderSocket->property("isAuthorized").toBool();

  if (type == "auth" && !isAuthorized)
    handleAuthRequest(senderSocket, json);
  else if (type == "change_password")
    handleChangePasswordRequest(senderSocket, json);
  else if (type == "message" && isAuthorized)
    handleChatMessage(senderSocket, json);
  else {
    spdlog::error("({}) Invalid packet sequence or unknown type.",
                  senderSocket->peerAddress().toString().toStdString());

    senderSocket->disconnectFromHost();
  }
}

void ChatServer::onClientDisconnected() {
  const auto senderSocket = qobject_cast<QTcpSocket *>(sender());
  if (!senderSocket)
    return;

  std::string ip = senderSocket->peerAddress().toString().toStdString();

  m_clients.removeAll(senderSocket);
  spdlog::info("Client disconnected: {}", ip);

  senderSocket->deleteLater();
}

void ChatServer::handleAuthRequest(QTcpSocket *senderSocket,
                                   const QJsonObject &json) {
  if (json["type"].toString() != "auth") {
    senderSocket->disconnectFromHost();
    return;
  }

  const QString login = json["login"].toString();
  const QString password = json["password"].toString();

  const auto hash = QString(
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256)
          .toHex());

  auto authResult = m_dbManager->checkAuth(login, hash);

  if (!authResult.isValid) {
    spdlog::error("Auth failed for login: {}", login.toStdString());
    senderSocket->disconnectFromHost();

    return;
  }

  senderSocket->setProperty("login", login);
  senderSocket->setProperty("nickname", authResult.nickname);

  if (authResult.requiresPasswordChange) {
    spdlog::warn("User '{}' needs to change password.", login.toStdString());

    QJsonObject response;
    response["type"] = "auth_response";
    response["status"] = "force_change";
    response["text"] = "You must change your password to continue.";

    const QByteArray outData = CryptoHelper::autoProcess(
        QJsonDocument(response).toJson(QJsonDocument::Compact));
    senderSocket->write(outData);
  } else {
    senderSocket->setProperty("isAuthorized", true);
    spdlog::info("User '{}' authorized successfully as '{}'",
                 login.toStdString(), authResult.nickname.toStdString());

    QJsonObject successMsg;
    successMsg["type"] = "auth_response";
    successMsg["status"] = "success";
    successMsg["nick"] = "System";
    successMsg["text"] =
        "Auth successful. Welcome, " + authResult.nickname + "!";

    const QByteArray outData = CryptoHelper::autoProcess(
        QJsonDocument(successMsg).toJson(QJsonDocument::Compact));
    senderSocket->write(outData);
  }
}

void ChatServer::handleChatMessage(const QTcpSocket *senderSocket,
                                   const QJsonObject &json) {
  if (json["type"].toString() != "message")
    return;

  const QString nickname = senderSocket->property("nickname").toString();

  QJsonObject outJson;
  outJson["nick"] = nickname;
  outJson["text"] = json["text"].toString();

  QByteArray outData = QJsonDocument(outJson).toJson(QJsonDocument::Compact);
  outData = CryptoHelper::autoProcess(outData);

  for (QTcpSocket *socket : m_clients) {
    if (socket != senderSocket && socket->property("isAuthorized").toBool()) {
      socket->write(outData);
    }
  }
}

void ChatServer::handleChangePasswordRequest(QTcpSocket *senderSocket,
                                             const QJsonObject &json) {
  const QString login = senderSocket->property("login").toString();

  if (login.isEmpty()) {
    spdlog::error("Password change attempt without login.");
    senderSocket->disconnectFromHost();
    return;
  }

  const bool isAuthorized = senderSocket->property("isAuthorized").toBool();

  const QString newPassword = json["new_password"].toString();
  if (newPassword.isEmpty())
    return;

  if (isAuthorized) {
    const QString oldPassword = json["old_password"].toString();
    const auto hash =
        QString(QCryptographicHash::hash(oldPassword.toUtf8(),
                                         QCryptographicHash::Sha256)
                    .toHex());

    if (!m_dbManager->checkAuth(login, hash).isValid) {
      const QJsonObject response{{"type", "error"},
                                 {"text", "Wrong old password"}};
      senderSocket->write(CryptoHelper::autoProcess(
          QJsonDocument(response).toJson(QJsonDocument::Compact)));
      return;
    }
  }

  if (m_dbManager->setPassword(login, newPassword, true)) {
    spdlog::info("User '{}' successfully updated their password.",
                 login.toStdString());

    QJsonObject response;

    if (!isAuthorized) {
      senderSocket->setProperty("isAuthorized", true);

      const QString nickname = senderSocket->property("nickname").toString();

      response["type"] = "auth_response";
      response["status"] = "success";
      response["nick"] = "System";
      response["text"] =
          "Password changed successfully. Welcome, " + nickname + "!";
    } else {
      response["type"] = "system_message";
      response["text"] = "Your password has been successfully updated.";
    }

    const QByteArray outData = CryptoHelper::autoProcess(
        QJsonDocument(response).toJson(QJsonDocument::Compact));
    senderSocket->write(outData);
  } else {
    spdlog::error("Database error while changing password for '{}'.",
                  login.toStdString());
    senderSocket->disconnectFromHost();
  }
}
