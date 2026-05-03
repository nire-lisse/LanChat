#include "ChatClient.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSslConfiguration>

#include <iostream>
#include <print>

#include "Commands/ClientCommandRegistry.h"
#include "NetworkUtils.h"

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
  QDataStream in(m_socket);
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

      if (type == "auth_response")
        handleAuthResponse(json);
      else if (type == "message")
        handleIncomingMessage(json);
      else if (type == "system_message")
        handleSystemMessage(json);
      else if (type == "history_response")
        handleHistoryResponse(json);
      else if (type == "message_edited")
        handleMessageEdited(json);
      else if (type == "message_pinned")
        handleMessagePinned(json);
      else if (type == "room_list")
        handleRoomList(json);
      else if (type == "join_response")
        handleJoinResponse(json);
      else if (type == "error")
        std::println("[Server Error] {}",
                     json["text"].toString().toStdString());
      else
        std::println("[Warning] Unknown packet type: {}", type.toStdString());
    } else {
      std::println("[System]: Error decrypting/parsing message");
    }

    in.startTransaction();
    in >> data;
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

  NetworkUtils::sendJson(m_socket, authMsg);
}

void ChatClient::sendChangePassword(const QString &newPassword,
                                    const QString &oldPassword) const {
  QJsonObject msg{{"type", "change_password"}, {"new_password", newPassword}};

  if (!oldPassword.isEmpty()) {
    msg["old_password"] = oldPassword;
  }

  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::sendChatMessage(const QString &text) const {
  const QJsonObject message{
      {"type", "message"}, {"text", text}};

  NetworkUtils::sendJson(m_socket, message);
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
  printMessage(json);
}

void ChatClient::handleSystemMessage(const QJsonObject &json) {
  std::println("[System] {}", json["text"].toString().toStdString());
}

void ChatClient::sendEditMessage(const qint64 messageId,
                                 const QString &newText) const {
  const QJsonObject msg{{"type", "edit_message"},
                        {"id", messageId},
                        {"text", newText}};

  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::sendPinMessage(const qint64 messageId,
                                const bool isPinned) const {
  const QJsonObject msg{{"type", "pin_message"},
                        {"id", messageId},
                        {"is_pinned", isPinned}};

  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::sendHistoryRequest(const qint64 beforeId) const {
  QJsonObject msg{{"type", "get_history"}};

  if (beforeId > 0)
    msg["before_id"] = beforeId;

  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::printMessage(const QJsonObject &msgJson) {
  const qint64 id = msgJson["id"].toVariant().toLongLong();
  const QString nick = msgJson["nick"].toString();
  const QString text = msgJson["text"].toString();

  QString timeStr = "";
  if (msgJson.contains("sent_at")) {
    const QDateTime sentAt =
        QDateTime::fromString(msgJson["sent_at"].toString(), Qt::ISODate);
    timeStr = sentAt.toLocalTime().toString("HH:mm");
  }

  const QString pinMarker = msgJson["is_pinned"].toBool() ? " [📌 PINNED]" : "";

  const QString editMarker = msgJson.contains("updated_at") ? " (edited)" : "";

  std::println("[ID:{}] [{}] {}{}: {}{}", id, timeStr.toStdString(),
               nick.toStdString(), pinMarker.toStdString(), text.toStdString(),
               editMarker.toStdString());
}

void ChatClient::handleHistoryResponse(const QJsonObject &json) {
  if (json.contains("pinned_messages")) {
    const QJsonArray pinned = json["pinned_messages"].toArray();
    if (!pinned.isEmpty()) {
      std::println("--- Pinned Messages ---");

      for (const auto &val : pinned)
        printMessage(val.toObject());

      std::println("-----------------------");
    }
  }

  const QJsonArray messages = json["messages"].toArray();
  for (const auto &val : messages)
    printMessage(val.toObject());

  std::println("--------------------");
}

void ChatClient::handleMessageEdited(const QJsonObject &json) {
  const qint64 id = json["id"].toVariant().toLongLong();
  const QString text = json["text"].toString();
  std::println("[System] Message ID:{} was edited. New text: {}", id,
               text.toStdString());
}

void ChatClient::handleMessagePinned(const QJsonObject &json) {
  const qint64 id = json["id"].toVariant().toLongLong();

  if (json["is_pinned"].toBool())
    std::println("[System] Message ID:{} was PINNED.", id);
  else
    std::println("[System] Message ID:{} was UNPINNED.", id);
}

void ChatClient::sendGetRoomsRequest() const {
  const QJsonObject msg{{"type", "get_rooms"}};
  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::sendJoinRoomRequest(const QString &target) const {
  const QJsonObject msg{{"type", "join_room"}, {"target", target}};
  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::handleRoomList(const QJsonObject &json) {
  const QJsonArray rooms = json["rooms"].toArray();
  std::println("\n--- Available Rooms ---");
  for (const auto &val : rooms) {
    const QJsonObject r = val.toObject();
    const QString name = r["name"].toString();
    const int type = r["type"].toInt();
    const QString desc = r["description"].toString();
    const int unread = r["unread"].toInt();

    QString typeStr = (type == 0)   ? "Public"
                      : (type == 1) ? "Private"
                                    : "Direct";

    if (type == 2)
      continue;

    QString unreadStr = unread > 0 ? QString(" (+%1 unread)").arg(unread) : "";

    std::println("[{}] {} - {}{}", typeStr.toStdString(), name.toStdString(),
                 desc.toStdString(), unreadStr.toStdString());
  }
  std::println("-----------------------");
}

void ChatClient::handleJoinResponse(const QJsonObject &json) {
  m_currentRoomId = json["room_id"].toInt();
  m_currentRoomName = json["target"].toString();

  for (int i = 0; i < 50; ++i)
    std::println("");

  std::println("=== Joined Room: {} ===", m_currentRoomName.toStdString());

  handleHistoryResponse(json);
}

void ChatClient::sendCreateRoomRequest(const QString &name, int type, const QString &description) const {
  const QJsonObject msg{
        {"type", "create_room"},
        {"name", name},
        {"room_type", type},
        {"description", description}
  };

  NetworkUtils::sendJson(m_socket, msg);
}

void ChatClient::sendInviteRequest(const QString &targetLogin) const {
  const QJsonObject msg{
        {"type", "invite_user"},
        {"target_login", targetLogin}
  };
  NetworkUtils::sendJson(m_socket, msg);
}

