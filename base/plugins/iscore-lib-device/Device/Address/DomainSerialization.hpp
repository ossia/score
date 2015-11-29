#pragma once

#include <QJsonObject>
#include <QString>

#include "Device/Address/Domain.hpp"

QJsonObject DomainToJson(const iscore::Domain& d);
iscore::Domain JsonToDomain(const QJsonObject& obj, const QString& t);
