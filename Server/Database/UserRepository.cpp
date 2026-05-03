#include "UserRepository.h"

#include <QCryptographicHash>
#include <QSqlError>
#include <QSqlQuery>
#include <spdlog/spdlog.h>

UserRepository::AuthResult UserRepository::checkAuth(const QString &login,
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

bool UserRepository::addUser(const QString &login, const QString &password,
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

QList<UserRepository::UserRecord> UserRepository::getUsers() {
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

bool UserRepository::setNickname(const QString &login,
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

bool UserRepository::setPassword(const QString &login,
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

bool UserRepository::deleteUser(const QString &login) {
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

int UserRepository::getIdByLogin(const QString &login) {
  QSqlQuery query;
  query.prepare(
      "SELECT id FROM users WHERE login = :login AND deleted_at IS NULL");
  query.bindValue(":login", login);

  if (query.exec() && query.next())
    return query.value(0).toInt();

  return -1;
}