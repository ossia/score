#pragma once
#include "Value.hpp"

// We require the type to crrectly read back (e.g. int / float / char)
// and as an optimization, since we may need it multiple times,
// we chose to leave the caller save it however he wants. Hence the specific API.
iscore::Value JsonToValue(const QJsonValue& val, QMetaType::Type t);
QJsonValue ValueToJson(const iscore::Value&);
