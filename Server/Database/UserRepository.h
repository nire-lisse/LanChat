#pragma once
#include <QString>

class UserRepository {
public:
  struct UserRecord {
    int id;
    QString login;
    QString nickname;
  };

  struct AuthResult {
    bool isValid = false;
    int id = -1;
    QString nickname;
    bool requiresPasswordChange = false;
  };

  UserRepository() = default;
  ~UserRepository() = default;

  AuthResult checkAuth(const QString &login, const QString &hash);

  bool addUser(const QString &login, const QString &password,
               const QString &nickname);
  QList<UserRecord> getUsers();
  bool setNickname(const QString &login, const QString &newNickname);
  bool setPassword(const QString &login, const QString &newPassword,
                   bool isChangedByUser = false);
  bool deleteUser(const QString &login);

  int getIdByLogin(const QString &login);
};
