#pragma once
#include "AddressSettings.hpp"

QJsonObject DomainToJson(const iscore::Domain& d);
iscore::Domain JsonToDomain(const QJsonObject& obj, QMetaType::Type t);
