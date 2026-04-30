#include "ChatServer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

#include "DatabaseManager.h"
#include "spdlog/spdlog.h"

void setupSsl(QSslServer *server) {
  QFile certFile("server.crt");
  QFile keyFile("server.key");

  if (certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly)) {
    const QSslCertificate cert(&certFile, QSsl::Pem);
    const QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(
        QSslSocket::VerifyNone); // Не вимагаємо сертифікат від клієнта

    server->setSslConfiguration(sslConfig);
    spdlog::info("SSL Certificates loaded!");
  } else {
    spdlog::error("Failed to load server.crt or server.key");
  }
}

ChatServer::ChatServer(const quint16 port, DatabaseManager *db, QObject *parent)
    : QObject(parent) {
  m_dbManager = db;

  m_server = new QSslServer(this);
  connect(m_server, &QSslServer::pendingConnectionAvailable, this,
          &ChatServer::onNewConnection);

  setupSsl(m_server);

  if (m_server->listen(QHostAddress::Any, port))
    spdlog::info("Server started on port {}", port);
  else
    spdlog::error("Server failed: {}", m_server->errorString().toStdString());
}

ChatServer::~ChatServer() {
  for (QSslSocket *socket : m_clients) {
    socket->close();
    socket->deleteLater();
  }
  m_server->close();
}

void ChatServer::onNewConnection() {
  QSslSocket *clientSocket =
      qobject_cast<QSslSocket *>(m_server->nextPendingConnection());
  m_clients.append(clientSocket);

  const auto clientAddress = clientSocket->peerAddress();

  spdlog::info(">>> New connection from: {}",
               clientAddress.toString().toStdString());

  connect(clientSocket, &QSslSocket::readyRead, this,
          &ChatServer::onClientReadyRead);
  connect(clientSocket, &QSslSocket::disconnected, this,
          &ChatServer::onClientDisconnected);

  connect(clientSocket, &QAbstractSocket::errorOccurred, this,
          [clientSocket](QAbstractSocket::SocketError socketError) {
            if (socketError != QAbstractSocket::RemoteHostClosedError) {
              spdlog::warn("Socket error for {}: {}",
                           clientSocket->peerAddress().toString().toStdString(),
                           clientSocket->errorString().toStdString());
            }
          });
}

void ChatServer::onClientReadyRead() {
  const auto senderSocket = qobject_cast<QSslSocket *>(sender());
  if (!senderSocket)
    return;

  const QByteArray data = senderSocket->readAll();

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

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
  const auto senderSocket = qobject_cast<QSslSocket *>(sender());
  if (!senderSocket)
    return;

  std::string ip = senderSocket->peerAddress().toString().toStdString();

  m_clients.removeAll(senderSocket);
  spdlog::info("Client disconnected: {}", ip);

  senderSocket->deleteLater();
}

void ChatServer::handleAuthRequest(QSslSocket *senderSocket,
                                   const QJsonObject &json) {
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

    const QJsonObject response{
        {"type", "auth_response"},
        {"status", "force_change"},
        {"text", "You must change your password to continue."}};

    senderSocket->write(QJsonDocument(response).toJson(QJsonDocument::Compact));
  } else {
    senderSocket->setProperty("isAuthorized", true);
    spdlog::info("User '{}' authorized successfully as '{}'",
                 login.toStdString(), authResult.nickname.toStdString());

    const QJsonObject successMsg{
        {"type", "auth_response"},
        {"status", "success"},
        {"nick", "System"},
        {"text", "Auth successful. Welcome, " + authResult.nickname + "!"}};

    senderSocket->write(
        QJsonDocument(successMsg).toJson(QJsonDocument::Compact));
  }
}

void ChatServer::handleChatMessage(const QSslSocket *senderSocket,
                                   const QJsonObject &json) {
  const QString nickname = senderSocket->property("nickname").toString();

  const QJsonObject outJson{{"type", "message"},
                            {"nick", nickname},
                            {"text", json["text"].toString()}};

  for (QSslSocket *socket : m_clients) {
    if (socket != senderSocket && socket->property("isAuthorized").toBool()) {
      socket->write(QJsonDocument(outJson).toJson(QJsonDocument::Compact));
    }
  }
}

void ChatServer::handleChangePasswordRequest(QSslSocket *senderSocket,
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
      senderSocket->write(
          QJsonDocument(response).toJson(QJsonDocument::Compact));
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

    senderSocket->write(QJsonDocument(response).toJson(QJsonDocument::Compact));
  } else {
    spdlog::error("Database error while changing password for '{}'.",
                  login.toStdString());
    senderSocket->disconnectFromHost();
  }
}
