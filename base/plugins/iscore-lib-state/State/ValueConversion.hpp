#pragma once
#include <State/Value.hpp>
#include <QChar>
#include <QDebug>
#include <QJsonValue>
#include <QString>
#include <QVariant>

class QStringList;

namespace State
{
namespace convert
{

template<typename To>
ISCORE_LIB_STATE_EXPORT To value(const State::Value& val);

template<>
ISCORE_LIB_STATE_EXPORT QVariant value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT QJsonValue value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT int value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT float value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT double value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT QString value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT QChar value(const State::Value& val);
template<>
ISCORE_LIB_STATE_EXPORT tuple_t value(const State::Value& val);

ISCORE_LIB_STATE_EXPORT bool convert(const State::Value& orig, State::Value& toConvert);

// Adornishments to allow to differentiate between different value types, e.g. 'a', ['a', 12], or "str" for a string.
ISCORE_LIB_STATE_EXPORT QString toPrettyString(const State::Value& val);

// We require the type to crrectly read back (e.g. int / float / char)
// and as an optimization, since we may need it multiple times,
// we chose to leave the caller save it however he wants. Hence the specific API.
ISCORE_LIB_STATE_EXPORT QString textualType(const State::Value& val); // For JSONValue serialization
ISCORE_LIB_STATE_EXPORT State::Value fromQVariant(const QVariant& val);
ISCORE_LIB_STATE_EXPORT State::Value fromQJsonValue(const QJsonValue& val); // Best effort
ISCORE_LIB_STATE_EXPORT State::Value fromQJsonValue(const QJsonValue& val, const QString& type);

ISCORE_LIB_STATE_EXPORT QString prettyType(const State::Value& val); // For display to the user, translated
ISCORE_LIB_STATE_EXPORT const QStringList& ValuePrettyTypesList(); // For display to the user, translated
}
}

inline QDebug operator<<(QDebug s, const State::Value& val)
{
    s << State::convert::toPrettyString(val);
    return s;
}
