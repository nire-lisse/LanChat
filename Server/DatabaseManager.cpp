#include "DatabaseManager.h"

#include <QCryptographicHash>
#include <QSqlError>
#include <QSqlQuery>
#include <iostream>
#include <print>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() {
  if (auto db = QSqlDatabase::database(); db.isOpen())
    db.close();
}

bool DatabaseManager::connectToDatabase(const QString &host,
                                        const QString &dbName,
                                        const QString &user,
                                        const QString &pass) {
  QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
  db.setHostName(host);
  db.setDatabaseName(dbName);
  db.setUserName(user);
  db.setPassword(pass);

  if (!db.open()) {
    std::cerr << "[DB Error] Failed to connect: "
              << db.lastError().text().toStdString() << std::endl;
    return false;
  }

  std::println("Database connected successfully.");
  return true;
}

DatabaseManager::AuthResult DatabaseManager::checkAuth(const QString &login,
                                                       const QString &hash) {
  AuthResult result;

  QSqlQuery query;
  query.prepare(
      "SELECT nickname, password_changed FROM users WHERE login = :login AND "
      "password_hash = :hash AND deleted_at IS NULL");
  query.bindValue(":login", login);
  query.bindValue(":hash", hash);

  if (query.exec() && query.next()) {
    result.isValid = true;
    result.nickname = query.value(0).toString();
    result.requiresPasswordChange = !query.value(1).toBool();
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

  return query.exec();
}

QList<DatabaseManager::UserRecord> DatabaseManager::getUsers() {
  QList<UserRecord> users;
  QSqlQuery query("SELECT id, login, nickname FROM users WHERE deleted_at IS "
                  "NULL ORDER BY id");

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

  return query.exec() && query.numRowsAffected() > 0;
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

  return query.exec() && query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteUser(const QString &login) {
  QSqlQuery query;
  query.prepare("UPDATE users SET deleted_at = CURRENT_TIMESTAMP WHERE login = "
                ":login AND deleted_at IS NULL");
  query.bindValue(":login", login);

  return query.exec() && query.numRowsAffected() > 0;
}