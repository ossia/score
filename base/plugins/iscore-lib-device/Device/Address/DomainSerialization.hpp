#pragma once

#include <qjsonobject.h>
#include <qstring.h>

#include "Device/Address/Domain.hpp"

QJsonObject DomainToJson(const iscore::Domain& d);
iscore::Domain JsonToDomain(const QJsonObject& obj, const QString& t);
