#include "ChatServer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

#include "Database/DatabaseManager.h"
#include "NetworkUtils.h"
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
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

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

  QDataStream in(senderSocket);
  in.setVersion(QDataStream::Qt_6_0);

  in.startTransaction();
  QByteArray data;
  in >> data;

  while (in.commitTransaction()) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error == QJsonParseError::NoError) {
      const QJsonObject json = doc.object();
      const QString type = json["type"].toString();

      bool isAuthorized = senderSocket->property("isAuthorized").toBool();

      if (type == "auth" && !isAuthorized)
        handleAuthRequest(senderSocket, json);
      else if (type == "change_password")
        handleChangePasswordRequest(senderSocket, json);
      else if (type == "message" && isAuthorized)
        handleChatMessage(senderSocket, json);
      else if (type == "get_history" && isAuthorized)
        handleHistoryRequest(senderSocket, json);
      else if (type == "edit_message" && isAuthorized)
        handleEditMessageRequest(senderSocket, json);
      else if (type == "pin_message" && isAuthorized)
        handlePinMessageRequest(senderSocket, json);
      else if (type == "join_room" && isAuthorized)
        handleJoinRoomRequest(senderSocket, json);
      else if (type == "get_rooms" && isAuthorized)
        handleGetRoomsRequest(senderSocket);
      else if (type == "create_room" && isAuthorized)
        handleCreateRoomRequest(senderSocket, json);
      else if (type == "invite_user" && isAuthorized)
        handleInviteRequest(senderSocket, json);
      else {
        spdlog::error("({}) Invalid packet sequence or unknown type.",
                      senderSocket->peerAddress().toString().toStdString());

        senderSocket->disconnectFromHost();
      }
    } else {
      spdlog::error("JSON parse error: {}", error.errorString().toStdString());
    }

    in.startTransaction();
    in >> data;
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
                                   const QJsonObject &json) const {
  const QString login = json["login"].toString();
  const QString password = json["password"].toString();

  const auto hash = QString(
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256)
          .toHex());

  auto authResult = m_dbManager->users().checkAuth(login, hash);

  if (!authResult.isValid) {
    spdlog::error("Auth failed for login: {}", login.toStdString());
    senderSocket->disconnectFromHost();

    return;
  }

  senderSocket->setProperty("user_id", authResult.id);
  senderSocket->setProperty("login", login);
  senderSocket->setProperty("nickname", authResult.nickname);

  if (authResult.requiresPasswordChange) {
    spdlog::warn("User '{}' needs to change password.", login.toStdString());

    const QJsonObject response{
        {"type", "auth_response"},
        {"status", "force_change"},
        {"text", "You must change your password to continue."}};

    NetworkUtils::sendJson(senderSocket, response);
  } else {
    senderSocket->setProperty("isAuthorized", true);
    senderSocket->setProperty("current_room_id", 1);

    m_dbManager->rooms().addMemberToRoom(1, authResult.id, RoomRole::Member);

    spdlog::info("User '{}' authorized successfully as '{}'",
                 login.toStdString(), authResult.nickname.toStdString());

    const QJsonObject successMsg{
        {"type", "auth_response"},
        {"status", "success"},
        {"nick", "System"},
        {"text", "Auth successful. Welcome, " + authResult.nickname + "!"}};

    NetworkUtils::sendJson(senderSocket, successMsg);

    handleGetRoomsRequest(senderSocket);

    QJsonArray history = m_dbManager->messages().getRoomHistory(1);
    QJsonArray pinned = m_dbManager->messages().getPinnedMessages(1);

    const QJsonObject historyMsg{{"type", "join_response"},
                                 {"room_id", 1},
                                 {"target", "Global"},
                                 {"messages", history},
                                 {"pinned_messages", pinned}};

    NetworkUtils::sendJson(senderSocket, historyMsg);
  }
}

void ChatServer::handleChatMessage(const QSslSocket *senderSocket,
                                   const QJsonObject &json) {
  const QString nickname = senderSocket->property("nickname").toString();
  const int userId = senderSocket->property("user_id").toInt();
  const int roomId = senderSocket->property("current_room_id").toInt();
  const QString text = json["text"].toString();

  QJsonObject savedData =
      m_dbManager->messages().saveMessage(roomId, userId, text);

  if (savedData.isEmpty()) {
    spdlog::error("Failed to save message from user ID: {}", userId);
    return;
  }

  const qint64 savedId = savedData["id"].toVariant().toLongLong();
  m_dbManager->rooms().updateLastReadMessage(userId, roomId, savedId);

  QJsonObject outJson{
      {"type", "message"}, {"id", savedData["id"].toVariant().toLongLong()},
      {"nick", nickname},  {"room_id", roomId},
      {"text", text},      {"sent_at", savedData["sent_at"].toString()},
      {"is_pinned", false}};

  if (savedData.contains("metadata")) {
    outJson["metadata"] = savedData["metadata"];
  }

  for (QSslSocket *socket : m_clients)
    if (socket != senderSocket && socket->property("isAuthorized").toBool())
      if (socket->property("current_room_id").toInt() == roomId)
        NetworkUtils::sendJson(socket, outJson);
}

void ChatServer::handleChangePasswordRequest(QSslSocket *senderSocket,
                                             const QJsonObject &json) const {
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

    if (!m_dbManager->users().checkAuth(login, hash).isValid) {
      const QJsonObject response{{"type", "error"},
                                 {"text", "Wrong old password"}};

      NetworkUtils::sendJson(senderSocket, response);
      return;
    }
  }

  if (m_dbManager->users().setPassword(login, newPassword, true)) {
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

    NetworkUtils::sendJson(senderSocket, response);
  } else {
    spdlog::error("Database error while changing password for '{}'.",
                  login.toStdString());

    senderSocket->disconnectFromHost();
  }
}

void ChatServer::handleHistoryRequest(QSslSocket *senderSocket,
                                      const QJsonObject &json) const {
  const qint64 beforeId = json["before_id"].toVariant().toLongLong();
  const int roomId = senderSocket->property("current_room_id").toInt();

  QJsonArray history = m_dbManager->messages().getRoomHistory(roomId, beforeId);

  const QJsonObject historyMsg{{"type", "history_response"},
                               {"messages", history}};

  NetworkUtils::sendJson(senderSocket, historyMsg);
}

void ChatServer::handleEditMessageRequest(const QSslSocket *senderSocket,
                                          const QJsonObject &json) {
  const int userId = senderSocket->property("user_id").toInt();
  const qint64 messageId = json["id"].toVariant().toLongLong();
  const QString newText = json["text"].toString();
  const int roomId = senderSocket->property("current_room_id").toInt();

  if (m_dbManager->messages().updateMessage(messageId, userId, newText)) {
    const QJsonObject outJson{
        {"type", "message_edited"}, {"id", messageId}, {"text", newText}};

    for (QSslSocket *socket : m_clients)
      if (socket->property("isAuthorized").toBool())
        if (socket->property("current_room_id").toInt() == roomId)
          NetworkUtils::sendJson(socket, outJson);
  }
}

void ChatServer::handlePinMessageRequest(const QSslSocket *senderSocket,
                                         const QJsonObject &json) {
  const qint64 messageId = json["id"].toVariant().toLongLong();
  const bool isPinned = json["is_pinned"].toBool();
  const int roomId = senderSocket->property("current_room_id").toInt();

  if (m_dbManager->messages().setMessagePinned(messageId, isPinned)) {
    const QJsonObject outJson{
        {"type", "message_pinned"}, {"id", messageId}, {"is_pinned", isPinned}};

    for (QSslSocket *socket : m_clients)
      if (socket->property("isAuthorized").toBool())
        if (socket->property("current_room_id").toInt() == roomId)
          NetworkUtils::sendJson(socket, outJson);
  }
}

void ChatServer::handleGetRoomsRequest(QSslSocket *senderSocket) const {
  const int userId = senderSocket->property("user_id").toInt();
  QList<RoomRecord> rooms = m_dbManager->rooms().getUserRooms(userId);

  QJsonArray roomsArray;
  for (const auto &r : rooms) {
    roomsArray.append(QJsonObject{{"id", r.id},
                                  {"name", r.name},
                                  {"type", static_cast<int>(r.type)},
                                  {"description", r.description},
                                  {"unread", r.unreadCount}});
  }

  const QJsonObject msg{{"type", "room_list"}, {"rooms", roomsArray}};
  NetworkUtils::sendJson(senderSocket, msg);
}

void ChatServer::handleJoinRoomRequest(QSslSocket *senderSocket,
                                       const QJsonObject &json) {
  const int userId = senderSocket->property("user_id").toInt();
  const QString target = json["target"].toString();
  int targetRoomId = -1;

  if (target.startsWith("@")) {
    const QString login = target.mid(1);
    const int targetUserId = m_dbManager->users().getIdByLogin(login);

    if (targetUserId == -1) {
      NetworkUtils::sendJson(senderSocket,
                             {{"type", "error"}, {"text", "User not found."}});
      return;
    }
    targetRoomId =
        m_dbManager->rooms().getOrCreateDirectRoom(userId, targetUserId);
  } else {
    targetRoomId = m_dbManager->rooms().getRoomIdByName(target);

    if (targetRoomId == -1) {
      NetworkUtils::sendJson(senderSocket,
                             {{"type", "error"}, {"text", "Room not found."}});
      return;
    }

    if (!m_dbManager->rooms().hasAccessToRoom(userId, targetRoomId)) {
      NetworkUtils::sendJson(senderSocket,
                             {{"type", "error"}, {"text", "Access denied."}});
      return;
    }
  }

  if (targetRoomId != -1) {
    m_dbManager->rooms().addMemberToRoom(targetRoomId, userId, RoomRole::Member);
    senderSocket->setProperty("current_room_id", targetRoomId);

    QJsonArray history = m_dbManager->messages().getRoomHistory(targetRoomId);
    QJsonArray pinned = m_dbManager->messages().getPinnedMessages(targetRoomId);

    if (!history.isEmpty()) {
      const qint64 lastMsgId = history.last().toObject()["id"].toVariant().toLongLong();
      m_dbManager->rooms().updateLastReadMessage(userId, targetRoomId, lastMsgId);
    }

    const QJsonObject response{{"type", "join_response"},
                               {"room_id", targetRoomId},
                               {"target", target},
                               {"messages", history},
                               {"pinned_messages", pinned}};
    NetworkUtils::sendJson(senderSocket, response);
  }
}

void ChatServer::handleCreateRoomRequest(QSslSocket *senderSocket,
                                         const QJsonObject &json) {
  const int userId = senderSocket->property("user_id").toInt();
  const QString name = json["name"].toString();
  const int type = json["room_type"].toInt();
  const QString description = json["description"].toString();

  const int newRoomId = m_dbManager->rooms().createRoom(
      name, static_cast<RoomType>(type), description, userId);

  if (newRoomId != -1) {
    NetworkUtils::sendJson(
        senderSocket, {{"type", "system_message"},
                       {"text", "Room '" + name + "' created successfully!"}});

    const QJsonObject joinJson{{"target", name}};
    handleJoinRoomRequest(senderSocket, joinJson);
  } else {
    NetworkUtils::sendJson(
        senderSocket,
        {{"type", "error"},
         {"text", "Failed to create room. Name might be taken."}});
  }
}

void ChatServer::handleInviteRequest(QSslSocket *senderSocket,
                                     const QJsonObject &json) {
  const int userId = senderSocket->property("user_id").toInt();
  const int roomId = senderSocket->property("current_room_id").toInt();
  const QString targetLogin = json["target_login"].toString();

  if (!m_dbManager->rooms().hasAccessToRoom(userId, roomId)) {
    NetworkUtils::sendJson(
        senderSocket,
        {{"type", "error"}, {"text", "You don't have access to this room."}});
    return;
  }

  const int targetUserId = m_dbManager->users().getIdByLogin(targetLogin);
  if (targetUserId == -1) {
    NetworkUtils::sendJson(
        senderSocket,
        {{"type", "error"}, {"text", "User '" + targetLogin + "' not found."}});
    return;
  }

  if (m_dbManager->rooms().addMemberToRoom(roomId, targetUserId,
                                           RoomRole::Member)) {
    NetworkUtils::sendJson(
        senderSocket, {{"type", "system_message"},
                       {"text", "User '" + targetLogin +
                                    "' was successfully added to the room!"}});
  } else {
    NetworkUtils::sendJson(
        senderSocket,
        {{"type", "error"},
         {"text", "Failed to add user. Maybe they are already in this room?"}});
  }
}