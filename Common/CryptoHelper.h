#pragma once

#include <QString>

class CryptoHelper {
public:
  static QByteArray process(const QByteArray &data, const QString &key);
};
