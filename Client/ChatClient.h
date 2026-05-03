#pragma once

#include <QJsonObject>
#include <QSocketNotifier>
#include <QSslSocket>
#include <QTextStream>
#include <memory>

class ClientCommandRegistry;
class ChatClient : public QObject {
  Q_OBJECT

public:
  explicit ChatClient(QObject *parent = nullptr);
  ~ChatClient() override;

  void connectToServer(const QString &ip, quint16 port,
                       const QString &login = "", const QString &password = "");

  void sendAuth(const QString &login, const QString &password) const;
  void sendChangePassword(const QString &newPassword,
                          const QString &oldPassword = "") const;
  void sendChatMessage(const QString &text) const;
  void sendEditMessage(qint64 messageId, const QString &newText) const;
  void sendPinMessage(qint64 messageId, bool isPinned) const;
  void sendHistoryRequest(qint64 beforeId = 0) const;
  void sendGetRoomsRequest() const;
  void sendJoinRoomRequest(const QString& target) const;
  void sendCreateRoomRequest(const QString &name, int type, const QString &description) const;
  void sendInviteRequest(const QString &targetLogin) const;

private slots:
  void onConnected();

  void onReadyRead();

  void onUserInput();

private:
  QSslSocket *m_socket;
  QSocketNotifier *m_stdinNotifier;
  QTextStream m_input;

  QString m_login;
  QString m_password;

  std::unique_ptr<ClientCommandRegistry> m_registry;
  bool m_isAuthorized = false;

  int m_currentRoomId = 1;
  QString m_currentRoomName = "Global";

  void handleAuthResponse(const QJsonObject &json);
  void handleIncomingMessage(const QJsonObject &json);
  void handleSystemMessage(const QJsonObject &json);
  void handleHistoryResponse(const QJsonObject &json);
  void handleMessageEdited(const QJsonObject &json);
  void handleMessagePinned(const QJsonObject &json);
  void handleRoomList(const QJsonObject &json);
  void handleJoinResponse(const QJsonObject &json);

  void printMessage(const QJsonObject &msgJson);
};
