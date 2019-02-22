// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Value.hpp"

#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/serialization/VariantSerialization.hpp>
#include <score/tools/std/Optional.hpp>

#include <boost/none_t.hpp>

#include <QChar>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

SCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const ossia::value& value)
{
  return State::convert::value<QJsonValue>(value);
}

namespace State::convert
{

template <>
QJsonValue value(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QJsonValue;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const impulse&) const
    {
      return {};
    }
    return_type operator()(int i) const
    {
      return i;
    }
    return_type operator()(float f) const
    {
      return f;
    }
    return_type operator()(bool b) const
    {
      return b;
    }
    return_type operator()(const std::string& s) const
    {
      return QString::fromStdString(s);
    }
    return_type operator()(char c) const
    {
      // Note : it is saved as a string but the actual type should be saved
      // also
      // so that the QChar can be recovered.
      return QString(c);
    }

    return_type operator()(vec2f t) const
    {
      return QJsonArray{t[0], t[1]};
    }
    return_type operator()(vec3f t) const
    {
      return QJsonArray{t[0], t[1], t[2]};
    }
    return_type operator()(vec4f t) const
    {
      return QJsonArray{t[0], t[1], t[2], t[3]};
    }

    return_type operator()(const list_t& t) const
    {
      QJsonArray arr;
      auto& strings = score::StringConstant();

      for (const auto& elt : t)
      {
        QJsonObject obj;
        obj[strings.Type] = textualType(elt);
        obj[strings.Value] = ossia::apply(*this, elt.v);
        arr.append(obj);
      }

      return arr;
    }
  };

  return ossia::apply(vis{}, val.v);
}
const QHash<QString, ossia::val_type> ValTypesMap{
    {QStringLiteral("Impulse"), ossia::val_type::IMPULSE},
    {QStringLiteral("Int"), ossia::val_type::INT},
    {QStringLiteral("Float"), ossia::val_type::FLOAT},
    {QStringLiteral("Bool"), ossia::val_type::BOOL},
    {QStringLiteral("String"), ossia::val_type::STRING},
    {QStringLiteral("Char"), ossia::val_type::CHAR},
    {QStringLiteral("Vec2f"), ossia::val_type::VEC2F},
    {QStringLiteral("Vec3f"), ossia::val_type::VEC3F},
    {QStringLiteral("Vec4f"), ossia::val_type::VEC4F},
    {QStringLiteral("List"), ossia::val_type::LIST},
    {QStringLiteral("Tuple"), ossia::val_type::LIST},
    {QStringLiteral("None"), ossia::val_type::NONE}};

static ossia::val_type which(const QString& val)
{
  auto it = ValTypesMap.find(val);
  SCORE_ASSERT(it != ValTypesMap.end()); // What happens if there is a
                                         // corrupt save file ?
  return static_cast<ossia::val_type>(*it);
}

static ossia::value fromQJsonValueImpl(const QJsonValue& val)
{
  switch (val.type())
  {
    case QJsonValue::Type::Bool:
      return val.toBool();
    case QJsonValue::Type::Double:
      return val.toDouble();
    case QJsonValue::Type::String:
      return val.toString().toStdString();
    case QJsonValue::Type::Array:
    {
      const auto& arr = val.toArray();
      std::vector<ossia::value> list;
      list.reserve(arr.size());

      for (const auto& v : arr)
      {
        list.push_back(fromQJsonValueImpl(v));
      }

      return list;
    }
    case QJsonValue::Type::Null:
    case QJsonValue::Type::Object:
    case QJsonValue::Type::Undefined:
    default:
      return ossia::value{};
  }
}

ossia::value fromQJsonValue(const QJsonValue& val)
{
  return fromQJsonValueImpl(val);
}

static ossia::value
fromQJsonValueImpl(const QJsonValue& val, ossia::val_type type)
{
  if (val.isNull())
  {
    if (type == ossia::val_type::IMPULSE)
      return ossia::value{State::impulse{}};
    else
      return ossia::value{};
  }

  switch (type)
  {
    case ossia::val_type::NONE:
      return ossia::value{};
    case ossia::val_type::IMPULSE:
      return ossia::value{State::impulse{}};
    case ossia::val_type::INT:
      return ossia::value{val.toInt()};
    case ossia::val_type::FLOAT:
      return ossia::value{val.toDouble()};
    case ossia::val_type::BOOL:
      return ossia::value{val.toBool()};
    case ossia::val_type::STRING:
      return ossia::value{val.toString().toStdString()};
    case ossia::val_type::CHAR:
    {
      auto str = val.toString();
      if (!str.isEmpty())
        return ossia::value{str[0].toLatin1()};
      return ossia::value{char{}};
    }
    case ossia::val_type::VEC2F:
    {
      auto json_arr = val.toArray();
      State::vec2f arr;
      int n = std::min((int)arr.size(), (int)json_arr.size());
      for (int i = 0; i < n; i++)
      {
        arr[i] = json_arr[i].toDouble();
      }

      return ossia::value{arr};
    }
    case ossia::val_type::VEC3F:
    {
      auto json_arr = val.toArray();
      State::vec3f arr;
      int n = std::min((int)arr.size(), (int)json_arr.size());
      for (int i = 0; i < n; i++)
      {
        arr[i] = json_arr[i].toDouble();
      }

      return ossia::value{arr};
    }
    case ossia::val_type::VEC4F:
    {
      auto json_arr = val.toArray();
      State::vec4f arr;
      int n = std::min((int)arr.size(), (int)json_arr.size());
      for (int i = 0; i < n; i++)
      {
        arr[i] = json_arr[i].toDouble();
      }

      return ossia::value{arr};
    }
    case ossia::val_type::LIST:
    {
      auto arr = val.toArray();
      std::vector<ossia::value> list;
      list.reserve(arr.size());

      auto& strings = score::StringConstant();

      Foreach(arr, [&](const auto& elt) {
        auto obj = elt.toObject();
        auto type_it = obj.find(strings.Type);
        auto val_it = obj.find(strings.Value);
        if (val_it != obj.end() && type_it != obj.end())
        {
          list.push_back(
              fromQJsonValueImpl(*val_it, which((*type_it).toString())));
        }
      });

      return ossia::value{list};
    }
    default:
      return ossia::value{};
  }
}

ossia::value fromQJsonValue(const QJsonValue& val, ossia::val_type which)
{
  return ossia::value{fromQJsonValueImpl(val, which)};
}

ossia::value fromQJsonValue(const QJsonValue& val, const QString& type)
{
  return fromQJsonValue(val, which(type));
}
}
