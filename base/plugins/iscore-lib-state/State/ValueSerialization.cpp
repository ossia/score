// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>

#include <QChar>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <boost/none_t.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/StringConstants.hpp>
#include <iscore/serialization/VariantSerialization.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "Value.hpp"

ISCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const ossia::value& value)
{
  return State::convert::value<QJsonValue>(value);
}
