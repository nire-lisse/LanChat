#include "ChatServer.h"

#include <iostream>
#include <print>

#include <QCryptographicHash>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QCoreApplication>

#include "CryptoHelper.h"

ChatServer::ChatServer(const quint16 port, QObject *parent) : QObject(parent) {
  QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
  QSettings settings(configPath, QSettings::IniFormat);

  QString dbHost = settings.value("Database/Host", "127.0.0.1").toString();
  QString dbName = settings.value("Database/Name", "lanchat").toString();
  QString dbUser = settings.value("Database/User", "chatserver").toString();
  QString dbPass = settings.value("Database/Password", "").toString();

  if (dbPass.isEmpty()) {
    std::cerr << "[Fatal Error] Database password is not set in config.ini!" << std::endl;
    exit(1);
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
  db.setHostName(dbHost);
  db.setDatabaseName(dbName);
  db.setUserName(dbUser);
  db.setPassword(dbPass);

  if (!db.open()) {
    std::cerr << "[DB Error] Failed to connect: "
              << db.lastError().text().toStdString() << std::endl;
  } else {
    std::println("Database connected successfully.");
  }

  m_server = new QTcpServer(this);
  connect(m_server, &QTcpServer::newConnection, this,
          &ChatServer::onNewConnection);

  if (m_server->listen(QHostAddress::Any, port)) {
    std::println("Server started on port {}", port);
  } else {
    std::cerr << "[Error] Server failed: "
              << m_server->errorString().toStdString() << std::endl;
  }
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

  std::println(">>> NEW CONNECTION from: {}",
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

  if (!senderSocket->property("isAuthorized").toBool()) {
    handleAuthRequest(senderSocket, json);
  } else {
    handleChatMessage(senderSocket, json);
  }
}

void ChatServer::onClientDisconnected() {
  const auto senderSocket = qobject_cast<QTcpSocket *>(sender());
  if (!senderSocket)
    return;

  std::string ip = senderSocket->peerAddress().toString().toStdString();

  m_clients.removeAll(senderSocket);
  std::println("Client disconnected: {}", ip);

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

  QSqlQuery query;
  query.prepare("SELECT nickname FROM users WHERE login = :login AND "
                "password_hash = :hash");
  query.bindValue(":login", login);
  query.bindValue(":hash", hash);

  if (query.exec() && query.next()) {
    const QString nickname = query.value(0).toString();
    senderSocket->setProperty("isAuthorized", true);
    senderSocket->setProperty("nickname", nickname);

    std::println("User '{}' authorized successfully as '{}'",
                 login.toStdString(), nickname.toStdString());

    QJsonObject successMsg;
    successMsg["nick"] = "System";
    successMsg["text"] = "Auth successful. Welcome, " + nickname + "!";
    const QByteArray outData = CryptoHelper::autoProcess(
        QJsonDocument(successMsg).toJson(QJsonDocument::Compact));

    senderSocket->write(outData);
  } else {
    std::println("Auth failed for login: {}", login.toStdString());
    senderSocket->disconnectFromHost();
  }
}

void ChatServer::handleChatMessage(const QTcpSocket *senderSocket,
                                   const QJsonObject &json) {
  if (json["type"].toString() != "message") {
    return;
  }

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
