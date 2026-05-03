#pragma once

#include "MessageRepository.h"
#include "RoomRepository.h"
#include "UserRepository.h"

#include <QSqlDatabase>

class DatabaseManager : public QObject {
  Q_OBJECT
public:
  explicit DatabaseManager(QObject *parent = nullptr);
  ~DatabaseManager() override;

  bool connectToDatabase(const QString &host, const QString &dbName,
                         const QString &user, const QString &pass,
                         const QString &encryptionKey);

  UserRepository& users() const { return *m_userRepo; }
  MessageRepository& messages() const { return *m_messageRepo; }
  RoomRepository& rooms() const { return *m_roomRepo; }

private:
  QString m_encryptionKey;
  std::unique_ptr<UserRepository> m_userRepo;
  std::unique_ptr<MessageRepository> m_messageRepo;
  std::unique_ptr<RoomRepository> m_roomRepo;

  void initTables();
};