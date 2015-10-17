#pragma once
#include <QJsonValue>
#include <QVariant>
#include <State/Value.hpp>

namespace iscore
{
namespace convert
{

template<typename To>
To value(const iscore::Value& val);

template<>
QVariant value(const iscore::Value& val);
template<>
QJsonValue value(const iscore::Value& val);
template<>
int value(const iscore::Value& val);
template<>
float value(const iscore::Value& val);
template<>
double value(const iscore::Value& val);
template<>
QString value(const iscore::Value& val);
template<>
QChar value(const iscore::Value& val);
template<>
tuple_t value(const iscore::Value& val);

bool convert(const iscore::Value& orig, iscore::Value& toConvert);

// Adornishments to allow to differentiate between different value types, e.g. 'a', ['a', 12], or "str" for a string.
QString toPrettyString(const iscore::Value& val);

// We require the type to crrectly read back (e.g. int / float / char)
// and as an optimization, since we may need it multiple times,
// we chose to leave the caller save it however he wants. Hence the specific API.
QString textualType(const iscore::Value& val); // For JSONValue serialization
iscore::Value toValue(const QVariant& val);
iscore::Value toValue(const QJsonValue& val, const QString& type);

QString prettyType(const iscore::Value& val); // For display to the user, translated
QStringList prettyTypes(); // For display to the user, translated
}
}
