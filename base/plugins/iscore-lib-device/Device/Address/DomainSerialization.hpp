#pragma once

#include <QJsonObject>
#include <QString>

#include <Device/Address/Domain.hpp>

namespace Device
{
QJsonObject DomainToJson(const Device::Domain& d);
Device::Domain JsonToDomain(const QJsonObject& obj, const QString& t);
}
