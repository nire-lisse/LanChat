#pragma once

#include <QJsonObject>

class MessageRepository {
public:
  MessageRepository(const QString &encryptionKey);
  ~MessageRepository() = default;

  QJsonObject saveMessage(int roomId, int senderId, const QString &text);
  bool updateMessage(qint64 messageId, int senderId, const QString &newText);

  QJsonArray getRoomHistory(int roomId, qint64 beforeId = 0, int limit = 50);

  bool setMessagePinned(qint64 messageId, bool isPinned);
  QJsonArray getPinnedMessages(int roomId);

private:
  QString m_encryptionKey;
};
