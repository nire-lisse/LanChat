#pragma once

#include "Database/DatabaseManager.h"

#include <QList>
#include <QSslServer>
#include <QSslSocket>

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

private:
  QSslServer *m_server;
  QList<QSslSocket *> m_clients;

  DatabaseManager *m_dbManager;

  void handleAuthRequest(QSslSocket *senderSocket, const QJsonObject &json) const;
  void handleChatMessage(const QSslSocket *senderSocket,
                         const QJsonObject &json);
  void handleChangePasswordRequest(QSslSocket *senderSocket,
                                   const QJsonObject &json) const;
  void handleHistoryRequest(QSslSocket *senderSocket, const QJsonObject &json) const;
  void handleEditMessageRequest(const QSslSocket *senderSocket,
                                const QJsonObject &json);
  void handlePinMessageRequest(const QSslSocket *senderSocket,
                               const QJsonObject &json);
  void handleJoinRoomRequest(QSslSocket *senderSocket, const QJsonObject &json);
  void handleGetRoomsRequest(QSslSocket *senderSocket) const;
  void handleCreateRoomRequest(QSslSocket *senderSocket, const QJsonObject &json);
  void handleInviteRequest(QSslSocket *senderSocket, const QJsonObject &json);
};
