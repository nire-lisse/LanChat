#pragma once

#include <QJsonObject>
#include <QSocketNotifier>
#include <QTcpSocket>
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

private slots:
  void onConnected();

  void onReadyRead();

  void onUserInput();

private:
  QTcpSocket *m_socket;
  QSocketNotifier *m_stdinNotifier;
  QTextStream m_input;

  QString m_login;
  QString m_password;

  std::unique_ptr<ClientCommandRegistry> m_registry;
  bool m_isAuthorized = false;

  void handleAuthResponse(const QJsonObject &json);
  void handleIncomingMessage(const QJsonObject &json);
  void handleSystemMessage(const QJsonObject &json);
};
