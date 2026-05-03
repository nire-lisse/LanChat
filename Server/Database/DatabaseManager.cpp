#include "DatabaseManager.h"

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

  m_messageRepo = std::make_unique<MessageRepository>(m_encryptionKey);
  m_userRepo = std::make_unique<UserRepository>();
  m_roomRepo = std::make_unique<RoomRepository>();

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

  if (!query.exec("CREATE EXTENSION IF NOT EXISTS pgcrypto"))
    spdlog::critical("[DB] Failed to enable pgcrypto: {}",
                  query.lastError().text().toStdString());

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
    )"))
    spdlog::critical("[DB] Failed to create users table: {}",
                  query.lastError().text().toStdString());

  if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS rooms (
            id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            name VARCHAR(255),
            type SMALLINT NOT NULL DEFAULT 0,
            description TEXT,
            created_by INT REFERENCES users(id) ON DELETE SET NULL,
            created_at TIMESTAMPTZ DEFAULT NOW()
        )
    )"))
    spdlog::critical("[DB] Failed to create rooms table: {}",
                  query.lastError().text().toStdString());

  query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_rooms_unique_name ON "
               "rooms(name) WHERE name IS NOT NULL");

  if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS room_members (
            room_id INT REFERENCES rooms(id) ON DELETE CASCADE,
            user_id INT REFERENCES users(id) ON DELETE CASCADE,
            role SMALLINT DEFAULT 0,
            last_read_message_id BIGINT DEFAULT 0,
            joined_at TIMESTAMPTZ DEFAULT NOW(),
            PRIMARY KEY (room_id, user_id)
        )
    )"))
    spdlog::critical("[DB] Failed to create room_members table: {}",
                  query.lastError().text().toStdString());

  if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS messages (
            id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            room_id INT REFERENCES rooms(id) ON DELETE CASCADE,
            sender_id INT REFERENCES users(id) ON DELETE CASCADE,
            text BYTEA,
            sent_at TIMESTAMPTZ DEFAULT NOW(),
            updated_at TIMESTAMPTZ DEFAULT NULL,
            is_deleted BOOLEAN DEFAULT FALSE,
            is_pinned BOOLEAN DEFAULT FALSE,
            metadata JSONB DEFAULT '{}'::jsonb
        )
    )"))
    spdlog::critical("[DB] Failed to create messages table: {}",
                  query.lastError().text().toStdString());

  query.exec("CREATE INDEX IF NOT EXISTS idx_messages_room_id ON "
             "messages(room_id, id DESC)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_pinned_messages ON "
             "messages(room_id) WHERE is_pinned = TRUE");

  query.exec(R"(
      INSERT INTO rooms (name, type, description)
      SELECT 'Global', 0, 'Main public chat room'
      WHERE NOT EXISTS (SELECT 1 FROM rooms WHERE name = 'Global' AND type = 0)
  )");
}