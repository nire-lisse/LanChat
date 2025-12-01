#include "CryptoHelper.h"

QByteArray CryptoHelper::process(const QByteArray &data, const QString &key) {
    QByteArray keyBytes = key.toUtf8();
    if (keyBytes.isEmpty()) {
        return data;
    }

    QByteArray result;
    result.reserve(data.length());

    for (int i = 0; i < data.length(); i++) {
        result.append(static_cast<char>(data[i] ^ keyBytes[i % keyBytes.length()]));
    }

    return result;
}
