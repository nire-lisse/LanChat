#pragma once

#include <QSocketNotifier>
#include <QTcpSocket>

class ChatClient : public QObject {
  Q_OBJECT

public:
  explicit ChatClient(QObject *parent = nullptr);

  void connectToServer(const QString &ip, quint16 port,
                       const QString &login = "", const QString &password = "");

private slots:
  void onConnected();

  void onReadyRead();

  void onUserInput();

private:
  QTcpSocket *m_socket;
  QSocketNotifier *m_stdinNotifier;
  QTextStream m_input;

  QString key;
  QString nickname;
  QString m_login;
  QString m_password;
};
