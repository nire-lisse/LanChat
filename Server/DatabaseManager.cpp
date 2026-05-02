#include "DatabaseManager.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include "spdlog/spdlog.h"

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() {
  if (auto db = QSqlDatabase::database(); db.isOpen())
    db.close();
}

bool DatabaseManager::connectToDatabase(const QString &host,
                                        const QString &dbName,
                                        const QString &user,
                                        const QString &pass,
                                        const QString &encryptionKey) {
  m_encryptionKey = encryptionKey;

  QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
  db.setHostName(host);
  db.setDatabaseName(dbName);
  db.setUserName(user);
  db.setPassword(pass);

  if (!db.open()) {
    spdlog::error("[DB] Failed to connect: {}",
                  db.lastError().text().toStdString());
    return false;
  }

  spdlog::info("[DB] Database connected successfully.");

  initTables();

  return true;
}

void DatabaseManager::initTables() {
  QSqlQuery query;

  if (!query.exec("CREATE EXTENSION IF NOT EXISTS pgcrypto")) {
    spdlog::error("Failed to enable pgcrypto: {}",
                  query.lastError().text().toStdString());
  }

  if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            login VARCHAR(255) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            nickname VARCHAR(255) NOT NULL,
            created_at TIMESTAMPTZ DEFAULT NOW(),
            deleted_at TIMESTAMPTZ DEFAULT NULL,
            password_changed BOOLEAN DEFAULT FALSE
        )
    )")) {
    spdlog::error("Failed to create users table: {}",
                  query.lastError().text().toStdString());
  }

  if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS messages (
            id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            room_id INT NOT NULL,
            sender_id INT NOT NULL,
            text BYTEA,
            sent_at TIMESTAMPTZ DEFAULT NOW(),
            updated_at TIMESTAMPTZ DEFAULT NULL,
            is_deleted BOOLEAN DEFAULT FALSE,
            is_pinned BOOLEAN DEFAULT FALSE,
            metadata JSONB DEFAULT '{}'::jsonb
        )
    )")) {
    spdlog::error("Failed to create messages table: {}",
                  query.lastError().text().toStdString());
  }

  query.exec("CREATE INDEX IF NOT EXISTS idx_messages_room_id ON "
             "messages(room_id, id DESC)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_pinned_messages ON "
             "messages(room_id) WHERE is_pinned = TRUE");
}

DatabaseManager::AuthResult DatabaseManager::checkAuth(const QString &login,
                                                       const QString &hash) {
  AuthResult result;

  QSqlQuery query;
  query.prepare("SELECT id, nickname, password_changed FROM users WHERE login "
                "= :login AND "
                "password_hash = :hash AND deleted_at IS NULL");
  query.bindValue(":login", login);
  query.bindValue(":hash", hash);

  if (query.exec() && query.next()) {
    result.isValid = true;
    result.id = query.value(0).toInt();
    result.nickname = query.value(1).toString();
    result.requiresPasswordChange = !query.value(2).toBool();
  } else if (query.lastError().isValid()) {
    spdlog::error("[DB] checkAuth error: {}",
                  query.lastError().text().toStdString());
  }

  return result;
}

bool DatabaseManager::addUser(const QString &login, const QString &password,
                              const QString &nickname) {
  const auto hash = QString(
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256)
          .toHex());

  QSqlQuery query;
  query.prepare("INSERT INTO users (login, password_hash, nickname) VALUES "
                "(:login, :hash, :nick)");
  query.bindValue(":login", login);
  query.bindValue(":hash", hash);
  query.bindValue(":nick", nickname);

  if (!query.exec()) {
    spdlog::error("[DB] addUser error: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return true;
}

QList<DatabaseManager::UserRecord> DatabaseManager::getUsers() {
  QList<UserRecord> users;
  QSqlQuery query("SELECT id, login, nickname FROM users WHERE deleted_at IS "
                  "NULL ORDER BY id");

  if (!query.exec()) {
    spdlog::error("[DB] getUsers error: {}",
                  query.lastError().text().toStdString());
    return users;
  }

  while (query.next())
    users.append({query.value(0).toInt(), query.value(1).toString(),
                  query.value(2).toString()});

  return users;
}

bool DatabaseManager::setNickname(const QString &login,
                                  const QString &newNickname) {
  QSqlQuery query;
  query.prepare("UPDATE users SET nickname = :nick WHERE login = :login");
  query.bindValue(":nick", newNickname);
  query.bindValue(":login", login);

  if (!query.exec()) {
    spdlog::error("[DB] setNickname error: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return query.numRowsAffected() > 0;
}

bool DatabaseManager::setPassword(const QString &login,
                                  const QString &newPassword,
                                  const bool isChangedByUser) {
  const auto hash = QString(
      QCryptographicHash::hash(newPassword.toUtf8(), QCryptographicHash::Sha256)
          .toHex());

  QSqlQuery query;
  query.prepare("UPDATE users SET password_hash = :hash, password_changed = "
                ":changed WHERE login = :login");
  query.bindValue(":hash", hash);
  query.bindValue(":changed", isChangedByUser);
  query.bindValue(":login", login);

  if (!query.exec()) {
    spdlog::error("[DB] setPassword error: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteUser(const QString &login) {
  QSqlQuery query;
  query.prepare("UPDATE users SET deleted_at = CURRENT_TIMESTAMP WHERE login = "
                ":login AND deleted_at IS NULL");
  query.bindValue(":login", login);

  if (!query.exec()) {
    spdlog::error("[DB] deleteUser error: {}",
                  query.lastError().text().toStdString());
    return false;
  }

  return query.numRowsAffected() > 0;
}

QJsonObject DatabaseManager::saveMessage(const int roomId, const int senderId,
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

QJsonArray DatabaseManager::getRoomHistory(const int roomId,
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
    if (!updatedAt.isNull()) {
      msg["updated_at"] = updatedAt.toDateTime().toString(Qt::ISODate);
    }

    QByteArray metadataBytes = query.value("metadata").toByteArray();
    if (!metadataBytes.isEmpty()) {
      QJsonParseError parseError;
      QJsonDocument metaDoc =
          QJsonDocument::fromJson(metadataBytes, &parseError);
      if (parseError.error == QJsonParseError::NoError && metaDoc.isObject()) {
        msg["metadata"] = metaDoc.object();
      }
    }

    tempMessages.prepend(msg);
  }

  for (const auto &msg : tempMessages) {
    history.append(msg);
  }

  return history;
}

bool DatabaseManager::setMessagePinned(const qint64 messageId,
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

QJsonArray DatabaseManager::getPinnedMessages(const int roomId) {
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

bool DatabaseManager::updateMessage(const qint64 messageId, const int senderId,
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