#pragma once

#include <QList>
#include <QString>

enum class RoomType : qint16 { Public = 0, Private = 1, Direct = 2 };

enum class RoomRole : qint16 { Member = 0, Admin = 1, Owner = 2 };

struct RoomRecord {
  int id;
  QString name;
  RoomType type;
  QString description;
  int unreadCount = 0;
};

class RoomRepository {
public:
  RoomRepository() = default;
  ~RoomRepository() = default;

  int createRoom(const QString &name, RoomType type, const QString &description,
                 int creatorId);
  bool addMemberToRoom(int roomId, int userId, RoomRole role);
  QList<RoomRecord> getPublicRooms();
  QList<RoomRecord> getUserRooms(int userId);
  int getRoomIdByName(const QString &name);

  int getOrCreateDirectRoom(int user1Id, int user2Id);
  bool hasAccessToRoom(int userId, int roomId);

  bool updateLastReadMessage(int userId, int roomId, qint64 messageId);
};