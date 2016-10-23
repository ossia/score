#pragma once

#include <QJsonObject>
#include <QString>

#include <Device/Address/Domain.hpp>

namespace Device
{
QJsonObject DomainToJson(const ossia::net::domain& d);
ossia::net::domain JsonToDomain(const QJsonObject& obj, const QString& t);
}
