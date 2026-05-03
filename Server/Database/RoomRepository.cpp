#include "RoomRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <spdlog/spdlog.h>

int RoomRepository::createRoom(const QString &name, RoomType type,
                               const QString &description,
                               const int creatorId) {
  QSqlQuery query;
  query.prepare(R"(
      INSERT INTO rooms (name, type, description, created_by)
      VALUES (:name, :type, :description, :creator)
      RETURNING id
  )");

  query.bindValue(":name", name.isEmpty() ? QVariant() : name);
  query.bindValue(":type", static_cast<qint16>(type));
  query.bindValue(":description", description);
  query.bindValue(":creator", creatorId > 0 ? creatorId : QVariant());

  if (query.exec() && query.next()) {
    const int newRoomId = query.value(0).toInt();

    if (creatorId > 0)
      addMemberToRoom(newRoomId, creatorId, RoomRole::Owner);

    return newRoomId;
  }

  spdlog::error("[DB] Failed to create room: {}",
                query.lastError().text().toStdString());
  return -1;
}

bool RoomRepository::addMemberToRoom(const int roomId, const int userId,
                                     RoomRole role) {
  QSqlQuery query;
  query.prepare(R"(
      INSERT INTO room_members (room_id, user_id, role)
      VALUES (:room_id, :user_id, :role)
      ON CONFLICT DO NOTHING
  )");

  query.bindValue(":room_id", roomId);
  query.bindValue(":user_id", userId);
  query.bindValue(":role", static_cast<qint16>(role));

  if (!query.exec()) {
    spdlog::error("[DB] Failed to add member: {}",
                  query.lastError().text().toStdString());
    return false;
  }
  return query.numRowsAffected() > 0;
}

QList<RoomRecord> RoomRepository::getPublicRooms() {
  QList<RoomRecord> rooms;
  QSqlQuery query(
      "SELECT id, name, type, description FROM rooms WHERE type = 0");

  if (!query.exec()) {
    spdlog::error("[DB] getPublicRooms error: {}",
                  query.lastError().text().toStdString());
    return rooms;
  }

  while (query.next())
    rooms.append({query.value(0).toInt(), query.value(1).toString(),
                  static_cast<RoomType>(query.value(2).toInt()),
                  query.value(3).toString()});

  return rooms;
}

QList<RoomRecord> RoomRepository::getUserRooms(const int userId) {
  QList<RoomRecord> rooms;
  QSqlQuery query;

  query.prepare(R"(
      SELECT r.id, r.name, r.type, r.description,
             (SELECT COUNT(*) FROM messages m
              WHERE m.room_id = r.id AND m.id > COALESCE(rm.last_read_message_id, 0)) as unread_count
      FROM rooms r
      LEFT JOIN room_members rm ON r.id = rm.room_id AND rm.user_id = :user_id
      WHERE r.type = 0 OR rm.user_id = :user_id
      ORDER BY r.id
  )");

  query.bindValue(":user_id", userId);

  if (!query.exec()) {
    spdlog::error("[DB] getUserRooms error: {}",
                  query.lastError().text().toStdString());
    return rooms;
  }

  while (query.next())
    rooms.append({query.value(0).toInt(), query.value(1).toString(),
                  static_cast<RoomType>(query.value(2).toInt()),
                  query.value(3).toString(), query.value(4).toInt()});

  return rooms;
}

int RoomRepository::getRoomIdByName(const QString &name) {
  QSqlQuery query;
  query.prepare("SELECT id FROM rooms WHERE name = :name");
  query.bindValue(":name", name);

  if (query.exec() && query.next())
    return query.value(0).toInt();

  return -1;
}

int RoomRepository::getOrCreateDirectRoom(const int user1Id,
                                          const int user2Id) {
  QSqlQuery query;

  query.prepare(R"(
      SELECT r.id
      FROM rooms r
      JOIN room_members rm1 ON r.id = rm1.room_id AND rm1.user_id = :u1
      JOIN room_members rm2 ON r.id = rm2.room_id AND rm2.user_id = :u2
      WHERE r.type = 2
  )");
  query.bindValue(":u1", user1Id);
  query.bindValue(":u2", user2Id);

  if (query.exec() && query.next()) {
    return query.value(0).toInt();
  }

  query.prepare("INSERT INTO rooms (type) VALUES (2) RETURNING id");
  if (query.exec() && query.next()) {
    const int roomId = query.value(0).toInt();
    addMemberToRoom(roomId, user1Id, RoomRole::Member);

    if (user1Id != user2Id)
      addMemberToRoom(roomId, user2Id, RoomRole::Member);

    return roomId;
  }
  return -1;
}

bool RoomRepository::hasAccessToRoom(const int userId, const int roomId) {
  QSqlQuery query;
  query.prepare(R"(
      SELECT r.type, rm.user_id
      FROM rooms r
      LEFT JOIN room_members rm ON r.id = rm.room_id AND rm.user_id = :user_id
      WHERE r.id = :room_id
  )");
  query.bindValue(":user_id", userId);
  query.bindValue(":room_id", roomId);

  if (query.exec() && query.next()) {
    const int type = query.value(0).toInt();
    const bool isMember = !query.value(1).isNull();

    return type == 0 || isMember;
  }

  return false;
}

bool RoomRepository::updateLastReadMessage(const int userId, const int roomId,
                                           const qint64 messageId) {
  QSqlQuery query;

  query.prepare(R"(
      UPDATE room_members
      SET last_read_message_id = GREATEST(last_read_message_id, :msg_id)
      WHERE user_id = :user_id AND room_id = :room_id
  )");
  query.bindValue(":msg_id", messageId);
  query.bindValue(":user_id", userId);
  query.bindValue(":room_id", roomId);

  return query.exec();
}