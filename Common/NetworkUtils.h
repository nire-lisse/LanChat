#pragma once

#include <QIODevice>
#include <QJsonObject>

namespace NetworkUtils {

inline void sendJson(QIODevice *device, const QJsonObject &json) {
  if (!device || !device->isOpen())
    return;

  const QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

  QDataStream out(device);
  out.setVersion(QDataStream::Qt_6_0);
  out << data;
}

} // namespace NetworkUtils