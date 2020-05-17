// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScoreDeviceLoader.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Domain.hpp>
#include <State/Value.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QFile>
#include <QIODevice>
#include <QDebug>

namespace Device
{

bool loadDeviceFromScoreJSON(const QString& filePath, Device::Node& node)
{
  QFile doc{filePath};
  if (!doc.open(QIODevice::ReadOnly))
  {
    qDebug() << "Erreur : Impossible d'ouvrir le ficher Device";
    doc.close();
    return false;
  }

  auto json = readJson(doc.readAll());
  if (!json.IsObject())
  {
    qDebug() << "Erreur : Impossible de charger le ficher Device";
    doc.close();
    return false;
  }

  doc.close();

  JSONObject::Deserializer des{json};
  des.writeTo(node);
  return true;
}
}
