#pragma once

#include "DatabaseManager.h"

#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

class ChatServer : public QObject {
  Q_OBJECT

public:
  explicit ChatServer(quint16 port, DatabaseManager *db,
                      QObject *parent = nullptr);

  ~ChatServer() override;

private slots:
  void onNewConnection();

  void onClientReadyRead();

  void onClientDisconnected();

  void handleAuthRequest(QTcpSocket *senderSocket, const QJsonObject &json);
  void handleChatMessage(const QTcpSocket *senderSocket,
                         const QJsonObject &json);
  void handleChangePasswordRequest(QTcpSocket *senderSocket,
                                   const QJsonObject &json);

private:
  QTcpServer *m_server;
  QList<QTcpSocket *> m_clients;

  DatabaseManager *m_dbManager;
};
