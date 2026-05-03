#include "MessageRepository.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <spdlog/spdlog.h>

MessageRepository::MessageRepository(const QString &encryptionKey)
    : m_encryptionKey(encryptionKey) {}

QJsonObject MessageRepository::saveMessage(const int roomId, const int senderId,
                                           const QString &text) {
  QJsonObject msgObj;
  QSqlQuery query;

  query.prepare(R"(
      INSERT INTO messages (room_id, sender_id, text)
      VALUES (:room_id, :sender_id, PGP_SYM_ENCRYPT(:text, :enc_key))
      RETURNING id, sent_at, metadata
  )");

  query.bindValue(":room_id", roomId);
  query.bindValue(":sender_id", senderId);
  query.bindValue(":text", text);
  query.bindValue(":enc_key", m_encryptionKey);

  if (!query.exec() || !query.next()) {
    spdlog::error("[DB] Failed to save message: {}",
                  query.lastError().text().toStdString());
    return msgObj;
  }

  msgObj["id"] = query.value("id").toLongLong();
  msgObj["sent_at"] = query.value("sent_at").toDateTime().toString(Qt::ISODate);

  QByteArray metadataBytes = query.value("metadata").toByteArray();
  if (!metadataBytes.isEmpty()) {
    QJsonParseError parseError;
    QJsonDocument metaDoc = QJsonDocument::fromJson(metadataBytes, &parseError);
    if (parseError.error == QJsonParseError::NoError && metaDoc.isObject()) {
      msgObj["metadata"] = metaDoc.object();
    }
  }

  return msgObj;
}

QJsonArray MessageRepository::getRoomHistory(const int roomId,
                                             const qint64 beforeId,
                                             const int limit) {
  QJsonArray history;
  QSqlQuery query;

  QString queryString = R"(
        SELECT m.id, PGP_SYM_DECRYPT(m.text::bytea, :enc_key) as text, u.nickname, m.is_pinned, m.sent_at, m.updated_at, m.metadata
        FROM messages m
        JOIN users u ON m.sender_id = u.id
        WHERE m.room_id = :room_id AND m.is_deleted = FALSE
  )";

  if (beforeId > 0) {
    queryString += " AND m.id < :before_id";
  }

  queryString += " ORDER BY m.id DESC LIMIT :limit";

  query.prepare(queryString);
  query.bindValue(":room_id", roomId);
  query.bindValue(":limit", limit);
  query.bindValue(":enc_key", m_encryptionKey);

  if (beforeId > 0) {
    query.bindValue(":before_id", beforeId);
  }

  if (!query.exec()) {
    spdlog::error("[DB] Failed to load history: {}",
                  query.lastError().text().toStdString());
    return history;
  }

  QList<QJsonObject> tempMessages;
  while (query.next()) {
    QJsonObject msg{
        {"type", "message"},
        {"id", query.value("id").toLongLong()},
        {"text", query.value("text").toString()},
        {"nick", query.value("nickname").toString()},
        {"is_pinned", query.value("is_pinned").toBool()},
        {"sent_at", query.value("sent_at").toDateTime().toString(Qt::ISODate)}};

    QVariant updatedAt = query.value("updated_at");
    if (!updatedAt.isNull())
      msg["updated_at"] = updatedAt.toDateTime().toString(Qt::ISODate);

    QByteArray metadataBytes = query.value("metadata").toByteArray();
    if (!metadataBytes.isEmpty()) {
      QJsonParseError parseError;
      QJsonDocument metaDoc =
          QJsonDocument::fromJson(metadataBytes, &parseError);
      if (parseError.error == QJsonParseError::NoError && metaDoc.isObject())
        msg["metadata"] = metaDoc.object();
    }

    tempMessages.prepend(msg);
  }

  for (const auto &msg : tempMessages)
    history.append(msg);

  return history;
}

bool MessageRepository::setMessagePinned(const qint64 messageId,
                                         const bool isPinned) {
  QSqlQuery query;
  query.prepare("UPDATE messages SET is_pinned = :is_pinned WHERE id = :id AND "
                "is_deleted = FALSE");
  query.bindValue(":is_pinned", isPinned);
  query.bindValue(":id", messageId);

  if (!query.exec()) {
    spdlog::error("[DB] setMessagePinned error: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return query.numRowsAffected() > 0;
}

QJsonArray MessageRepository::getPinnedMessages(const int roomId) {
  QJsonArray pinnedArray;
  QSqlQuery query;

  query.prepare(R"(
        SELECT m.id, PGP_SYM_DECRYPT(m.text::bytea, :enc_key) as text, u.nickname
        FROM messages m
        JOIN users u ON m.sender_id = u.id
        WHERE m.room_id = :room_id AND m.is_pinned = TRUE AND m.is_deleted = FALSE
        ORDER BY m.sent_at DESC
    )");
  query.bindValue(":room_id", roomId);
  query.bindValue(":enc_key", m_encryptionKey);

  if (!query.exec()) {
    spdlog::error("getPinnedMessages error: {}",
                  query.lastError().text().toStdString());
    return pinnedArray;
  }

  while (query.next()) {
    const QJsonObject msg{{"type", "pinned_message"},
                          {"id", query.value("id").toLongLong()},
                          {"text", query.value("text").toString()},
                          {"nick", query.value("nickname").toString()}};
    pinnedArray.append(msg);
  }

  return pinnedArray;
}

bool MessageRepository::updateMessage(const qint64 messageId,
                                      const int senderId,
                                      const QString &newText) {
  QSqlQuery query;

  query.prepare(R"(
      UPDATE messages
      SET text = PGP_SYM_ENCRYPT(:text, :enc_key), updated_at = NOW()
      WHERE id = :id AND sender_id = :sender_id AND is_deleted = FALSE
  )");

  query.bindValue(":text", newText);
  query.bindValue(":enc_key", m_encryptionKey);
  query.bindValue(":id", messageId);
  query.bindValue(":sender_id", senderId);

  if (!query.exec()) {
    spdlog::error("[DB] Failed to update message: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return query.numRowsAffected() > 0;
}