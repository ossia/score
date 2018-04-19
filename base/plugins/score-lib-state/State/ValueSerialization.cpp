// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Value.hpp"

#include <QChar>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>
#include <boost/none_t.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/serialization/VariantSerialization.hpp>
#include <score/tools/std/Optional.hpp>

SCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const ossia::value& value)
{
  return State::convert::value<QJsonValue>(value);
}
