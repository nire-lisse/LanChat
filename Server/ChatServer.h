#pragma once

#include <QDebug>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

class ChatServer : public QObject {
  Q_OBJECT

public:
  explicit ChatServer(quint16 port, QObject *parent = nullptr);

  ~ChatServer() override;

private slots:
  void onNewConnection();

  void onClientReadyRead();

  void onClientDisconnected();

private:
  QTcpServer *m_server;
  QList<QTcpSocket *> m_clients;
};
