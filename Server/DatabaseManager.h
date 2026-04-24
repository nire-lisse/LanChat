#pragma once

#include <QSqlDatabase>

class DatabaseManager : public QObject {
  Q_OBJECT
public:
  struct UserRecord {
    int id;
    QString login;
    QString nickname;
  };

  struct AuthResult {
    bool isValid = false;
    QString nickname;
    bool requiresPasswordChange = false;
  };

  explicit DatabaseManager(QObject *parent = nullptr);
  ~DatabaseManager() override;

  bool connectToDatabase(const QString &host, const QString &dbName,
                         const QString &user, const QString &pass);

  AuthResult checkAuth(const QString &login, const QString &hash);

  bool addUser(const QString &login, const QString &password,
               const QString &nickname);
  QList<UserRecord> getUsers();
  bool setNickname(const QString &login, const QString &newNickname);
  bool setPassword(const QString &login, const QString &newPassword,
                   bool isChangedByUser = false);
  bool deleteUser(const QString &login);
};