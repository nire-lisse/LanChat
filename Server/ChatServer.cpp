#include "ChatServer.h"

#include <iostream>
#include <print>

ChatServer::ChatServer(const quint16 port, QObject *parent) : QObject(parent) {
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

  for (QTcpSocket *socket : m_clients) {
    if (socket != senderSocket &&
        socket->state() == QAbstractSocket::ConnectedState) {
      socket->write(data);
    }
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
