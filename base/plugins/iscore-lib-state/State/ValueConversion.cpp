// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonArray>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QObject>
#include <ossia/detail/apply.hpp>

#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <iterator>

#include "Expression.hpp"
#include "ValueConversion.hpp"
#include <ossia/editor/value/value.hpp>
#include <State/Value.hpp>

namespace State
{
namespace convert
{

const std::array<const QString, 11> ValuePrettyTypes{
    {QObject::tr("Float"), QObject::tr("Int"), QObject::tr("Vec2f"),
     QObject::tr("Vec3f"), QObject::tr("Vec4f"), QObject::tr("Impulse"),
     QObject::tr("Bool"), QObject::tr("String"), QObject::tr("Tuple"),
     QObject::tr("Char"), QObject::tr("Container")}};

const std::array<std::pair<QString, ossia::val_type>, 10> ValuePrettyTypesPairArray{
    {std::make_pair(QObject::tr("Impulse"), ossia::val_type::IMPULSE),
     std::make_pair(QObject::tr("Int"), ossia::val_type::INT),
     std::make_pair(QObject::tr("Float"), ossia::val_type::FLOAT),
     std::make_pair(QObject::tr("Bool"), ossia::val_type::BOOL),
     std::make_pair(QObject::tr("String"), ossia::val_type::STRING),
     std::make_pair(QObject::tr("Char"), ossia::val_type::CHAR),
     std::make_pair(QObject::tr("Vec2f"), ossia::val_type::VEC2F),
     std::make_pair(QObject::tr("Vec3f"), ossia::val_type::VEC3F),
     std::make_pair(QObject::tr("Vec4f"), ossia::val_type::VEC4F),
     std::make_pair(QObject::tr("Tuple"), ossia::val_type::TUPLE)}};

template <>
QVariant value(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QVariant;
    return_type operator()() const
    {
      return QVariant{};
    }
    return_type operator()(const impulse&) const
    {
      return QVariant::fromValue(State::impulse{});
    }
    return_type operator()(int i) const
    {
      return QVariant::fromValue(i);
    }
    return_type operator()(float f) const
    {
      return QVariant::fromValue(f);
    }
    return_type operator()(bool b) const
    {
      return QVariant::fromValue(b);
    }
    return_type operator()(const QString& s) const
    {
      return QVariant::fromValue(s);
    }
    return_type operator()(const std::string& s) const
    {
      return operator()(QString::fromStdString(s));
    }
    return_type operator()(QChar c) const
    {
      return QVariant::fromValue(c);
    }
    return_type operator()(char c) const
    {
      return QVariant::fromValue(QChar(c));
    }

    return_type operator()(vec2f t) const
    {
      return QVector2D{t[0], t[1]};
    }
    return_type operator()(vec3f t) const
    {
      return QVector3D{t[0], t[1], t[2]};
    }
    return_type operator()(vec4f t) const
    {
      return QVector4D{t[0], t[1], t[2], t[3]};
    }
    return_type operator()(const tuple_t& t) const
    {
      QVariantList arr;
      arr.reserve(t.size());

      for (const auto& elt : t)
      {
        arr.push_back(ossia::apply(*this, elt.v));
      }

      return arr;
    }
  };

  return ossia::apply(vis{}, val.v);
}

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
    return_type operator()(const QString& s) const
    {
      return s;
    }
    return_type operator()(const std::string& s) const
    {
      return QString::fromStdString(s);
    }

    return_type operator()(QChar c) const
    {
      // Note : it is saved as a string but the actual type should be saved
      // also
      // so that the QChar can be recovered.
      return QString(c);
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

    return_type operator()(const tuple_t& t) const
    {
      QJsonArray arr;
      auto& strings = iscore::StringConstant();

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

QString textualType(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QString;
    return_type operator()() const
    {
      return QStringLiteral("None");
    }
    return_type operator()(impulse) const
    {
      return QStringLiteral("Impulse");
    }
    return_type operator()(int i) const
    {
      return QStringLiteral("Int");
    }
    return_type operator()(float f) const
    {
      return QStringLiteral("Float");
    }
    return_type operator()(bool b) const
    {
      return QStringLiteral("Bool");
    }
    return_type operator()(const QString& s) const
    {
      return QStringLiteral("String");
    }
    return_type operator()(const std::string& s) const
    {
      return QStringLiteral("String");
    }
    return_type operator()(QChar c) const
    {
      return QStringLiteral("Char");
    }
    return_type operator()(char c) const
    {
      return QStringLiteral("Char");
    }
    return_type operator()(vec2f t) const
    {
      return QStringLiteral("Vec2f");
    }
    return_type operator()(vec3f t) const
    {
      return QStringLiteral("Vec3f");
    }
    return_type operator()(vec4f t) const
    {
      return QStringLiteral("Vec4f");
    }
    return_type operator()(const tuple_t& t) const
    {
      return QStringLiteral("Tuple");
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
    {QStringLiteral("Tuple"), ossia::val_type::TUPLE},
    {QStringLiteral("None"), ossia::val_type::NONE}};

static ossia::val_type which(const QString& val)
{
  auto it = ValTypesMap.find(val);
  ISCORE_ASSERT(it != ValTypesMap.end()); // What happens if there is a
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
      State::tuple_t tuple;
      tuple.reserve(arr.size());

      for (const auto& v : arr)
      {
        tuple.push_back(fromQJsonValueImpl(v));
      }

      return tuple;
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
    case ossia::val_type::TUPLE:
    {
      auto arr = val.toArray();
      State::tuple_t tuple;
      tuple.reserve(arr.size());

      auto& strings = iscore::StringConstant();

      Foreach(arr, [&](const auto& elt) {
        auto obj = elt.toObject();
        auto type_it = obj.find(strings.Type);
        auto val_it = obj.find(strings.Value);
        if (val_it != obj.end() && type_it != obj.end())
        {
          tuple.push_back(
              fromQJsonValueImpl(*val_it, which((*type_it).toString())));
        }
      });

      return ossia::value{tuple};
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

QString prettyType(const ossia::value& val)
{
  const auto& impl = val.v;
  if ((std::size_t)impl.which() < ValuePrettyTypes.size())
    return ValuePrettyTypes.at(impl.which());
  else
    return ValuePrettyTypes.back();
}

const QStringList& ValuePrettyTypesList()
{
  static bool init = false;
  static QStringList lst;
  if (!init)
  {
    for (const auto& str : ValuePrettyTypes)
      lst.append(str);
    init = true;
  }
  return lst;
}

template <>
int value(const ossia::value& val)
{
  struct
  {
  public:
    using return_type = int;
    return_type operator()() const
    {
      return 0;
    }
    return_type operator()(const impulse&) const
    {
      return 0;
    }
    return_type operator()(int v) const
    {
      return v;
    }
    return_type operator()(float v) const
    {
      return v;
    }
    return_type operator()(bool v) const
    {
      return v;
    }
    return_type operator()(const QString& v) const
    {
      return QLocale::c().toInt(v);
    }
    return_type operator()(const std::string& v) const
    {
      return QLocale::c().toInt(QString::fromStdString(v));
    }
    return_type operator()(QChar v) const
    {
      return QLocale::c().toInt(QString(v));
    }
    return_type operator()(char v) const
    {
      return QLocale::c().toInt(QString(v));
    }
    return_type operator()(const vec2f& v) const
    {
      return 0;
    }
    return_type operator()(const vec3f& v) const
    {
      return 0;
    }
    return_type operator()(const vec4f& v) const
    {
      return 0;
    }
    return_type operator()(const tuple_t& v) const
    {
      return 0;
    }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template <>
float value(const ossia::value& val)
{
  struct
  {
  public:
    using return_type = float;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const impulse&) const
    {
      return {};
    }
    return_type operator()(int v) const
    {
      return v;
    }
    return_type operator()(float v) const
    {
      return v;
    }
    return_type operator()(bool v) const
    {
      return v;
    }
    return_type operator()(const QString& v) const
    {
      return QLocale::c().toFloat(v);
    }
    return_type operator()(const std::string& v) const
    {
      return operator()(QString::fromStdString(v));
    }
    return_type operator()(QChar v) const
    {
      return QLocale::c().toFloat(QString(v));
    }
    return_type operator()(char v) const
    {
      return QLocale::c().toFloat(QString(v));
    }
    return_type operator()(const vec2f& v) const
    {
      return 0;
    }
    return_type operator()(const vec3f& v) const
    {
      return 0;
    }
    return_type operator()(const vec4f& v) const
    {
      return 0;
    }
    return_type operator()(const tuple_t& v) const
    {
      return {};
    }
  } visitor{};

  return ossia::apply(visitor, val.v);
}
template <>
double value(const ossia::value& val)
{
  return (double)value<float>(val);
}

template <>
bool value(const ossia::value& val)
{
  struct
  {
  public:
    using return_type = bool;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const impulse&) const
    {
      return {};
    }
    return_type operator()(int v) const
    {
      return v;
    }
    return_type operator()(float v) const
    {
      return v;
    }
    return_type operator()(bool v) const
    {
      return v;
    }
    return_type operator()(const QString& v) const
    {
      auto& strings = iscore::StringConstant();

      return v == strings.lowercase_true || v == strings.True
             || v == strings.lowercase_yes || v == strings.Yes;
    }
    return_type operator()(const std::string& ve) const
    {
      auto& strings = iscore::StringConstant();

      auto v = QString::fromStdString(ve);
      return v == strings.lowercase_true || v == strings.True
             || v == strings.lowercase_yes || v == strings.Yes;
    }
    return_type operator()(QChar v) const
    {
      return v == 't' || v == 'T' || v == 'y' || v == 'Y';
    }
    return_type operator()(char v) const
    {
      return v == 't' || v == 'T' || v == 'y' || v == 'Y';
    }
    return_type operator()(const vec2f& v) const
    {
      return false;
    }
    return_type operator()(const vec3f& v) const
    {
      return false;
    }
    return_type operator()(const vec4f& v) const
    {
      return false;
    }
    return_type operator()(const tuple_t& v) const
    {
      return false;
    }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template <>
QChar value(const ossia::value& val)
{
  struct
  {
  public:
    using return_type = QChar;
    return_type operator()() const
    {
      return '-';
    }
    return_type operator()(const impulse&) const
    {
      return '-';
    }
    return_type operator()(int) const
    {
      return '-';
    }
    return_type operator()(float) const
    {
      return '-';
    }
    return_type operator()(bool v) const
    {
      return v ? 'T' : 'F';
    }
    return_type operator()(const QString& s) const
    {
      return !s.isEmpty() ? s[0] : '-';
    } // TODO boueeeff
    return_type operator()(const std::string& s) const
    {
      return !s.empty() ? s[0] : '-';
    } // TODO boueeeff
    return_type operator()(QChar v) const
    {
      return v.toLatin1();
    }
    return_type operator()(char v) const
    {
      return v;
    }
    return_type operator()(const vec2f& v) const
    {
      return '-';
    }
    return_type operator()(const vec3f& v) const
    {
      return '-';
    }
    return_type operator()(const vec4f& v) const
    {
      return '-';
    }
    return_type operator()(const tuple_t&) const
    {
      return '-';
    }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template <>
QString value(const ossia::value& val)
{
  struct
  {
    using return_type = QString;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const State::impulse&) const
    {
      return {};
    }
    return_type operator()(int i) const
    {
      return QLocale::c().toString(i);
    }
    return_type operator()(float f) const
    {
      return QLocale::c().toString(f);
    }
    return_type operator()(bool b) const
    {
      auto& strings = iscore::StringConstant();

      return b ? strings.lowercase_true : strings.lowercase_false;
    }
    return_type operator()(const QString& s) const
    {
      return s;
    }
    return_type operator()(const std::string& s) const
    {
      return QString::fromStdString(s);
    }
    return_type operator()(QChar c) const
    {
      return c;
    }
    return_type operator()(char c) const
    {
      return QChar(c);
    }
    return_type operator()(const vec2f& v) const
    {
      return {};
    }
    return_type operator()(const vec3f& v) const
    {
      return {};
    }
    return_type operator()(const vec4f& v) const
    {
      return {};
    }
    return_type operator()(const State::tuple_t& t) const
    {
      return {};
    }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template<int N, typename Vis>
std::array<float, N> string_to_vec(const std::string& s, const Vis& visitor)
{
  auto v = parseValue(s);

  if (v)
  {
    const auto& val = *v;

    if(auto t = val.target<tuple_t>())
      return visitor(*t);
  }

  return {};
}

template <>
vec2f value(const ossia::value& val)
{
  struct vis
  {
    using return_type = vec2f;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const State::impulse&) const
    {
      return {};
    }
    return_type operator()(int i) const
    {
      return {{float(i)}};
    }
    return_type operator()(float f) const
    {
      return {{f}};
    }
    return_type operator()(bool b) const
    {
      return {{float(b)}};
    }
    return_type operator()(const std::string& s) const
    {
      return string_to_vec<2>(s, *this);
    }
    return_type operator()(QChar c) const
    {
      return {};
    }
    return_type operator()(char c) const
    {
      return {};
    }
    return_type operator()(const vec2f& v) const
    {
      return v;
    }
    return_type operator()(const vec3f& v) const
    {
      return {{v[0], v[1]}};
    }
    return_type operator()(const vec4f& v) const
    {
      return {{v[0], v[1]}};
    }
    return_type operator()(const State::tuple_t& t) const
    {
      const int n = t.size();
      const int n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (int i = 0; i < std::min(n, n_2); i++)
      {
        v[i] = value<float>(t[i]);
      }
      return v;
    }
  };

  return ossia::apply(vis{}, val.v);
}

template <>
vec3f value(const ossia::value& val)
{
  struct vis
  {
    using return_type = vec3f;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const State::impulse&) const
    {
      return {};
    }
    return_type operator()(int i) const
    {
      return {{float(i)}};
    }
    return_type operator()(float f) const
    {
      return {{f}};
    }
    return_type operator()(bool b) const
    {
      return {{float(b)}};
    }

    return_type operator()(const std::string& s) const
    {
      return string_to_vec<3>(s, *this);
    }
    return_type operator()(char c) const
    {
      return {};
    }
    return_type operator()(const vec2f& v) const
    {
      return {{v[0], v[1]}};
    }
    return_type operator()(const vec3f& v) const
    {
      return v;
    }
    return_type operator()(const vec4f& v) const
    {
      return {{v[0], v[1], v[2]}};
    }
    return_type operator()(const State::tuple_t& t) const
    {
      const int n = t.size();
      const int n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (int i = 0; i < std::min(n, n_2); i++)
      {
        v[i] = value<float>(t[i]);
      }
      return v;
    }
  };

  return ossia::apply(vis{}, val.v);
}

template <>
vec4f value(const ossia::value& val)
{
  struct vis
  {
    using return_type = vec4f;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const State::impulse&) const
    {
      return {};
    }
    return_type operator()(int i) const
    {
      return {{float(i)}};
    }
    return_type operator()(float f) const
    {
      return {{f}};
    }
    return_type operator()(bool b) const
    {
      return {{float(b)}};
    }
    return_type operator()(const std::string& s) const
    {
      return string_to_vec<4>(s, *this);
    }
    return_type operator()(char c) const
    {
      return {};
    }
    return_type operator()(const vec2f& v) const
    {
      return {{v[0], v[1]}};
    }
    return_type operator()(const vec3f& v) const
    {
      return {{v[0], v[1], v[2]}};
    }
    return_type operator()(const vec4f& v) const
    {
      return v;
    }
    return_type operator()(const State::tuple_t& t) const
    {
      const int n = t.size();
      const int n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (int i = 0; i < std::min(n, n_2); i++)
      {
        v[i] = value<float>(t[i]);
      }
      return v;
    }
  };

  return ossia::apply(vis{}, val.v);
}

template <>
tuple_t value(const ossia::value& val)
{
  struct vis
  {
    using return_type = tuple_t;
    return_type operator()() const
    {
      return {};
    }
    return_type operator()(const State::impulse&) const
    {
      return {impulse{}};
    }
    return_type operator()(int i) const
    {
      return {i};
    }
    return_type operator()(float f) const
    {
      return {f};
    }
    return_type operator()(bool b) const
    {
      return {b};
    }
    return_type operator()(const std::string& s) const
    {
      auto v = parseValue(s);

      if (v)
      {
        if(auto t = v->target<tuple_t>())
          return *t;
      }

      return {s};
    }
    return_type operator()(char c) const
    {
      return {};
    }
    return_type operator()(const vec2f& v) const
    {
      return {{v[0], v[1]}};
    }
    return_type operator()(const vec3f& v) const
    {
      return {{v[0], v[1], v[2]}};
    }
    return_type operator()(const vec4f& v) const
    {
      return {{v[0], v[1], v[2], v[3]}};
    }
    return_type operator()(const State::tuple_t& t) const
    {
      return t;
    }
  };

  return ossia::apply(vis{}, val.v);
}

template <>
std::string value(const ossia::value& val)
{
  return value<QString>(val).toStdString();
}

template <>
char value(const ossia::value& val)
{
  return value<QChar>(val).toLatin1();
}

QString toPrettyString(const ossia::value& val)
{
  struct vis
  {
    QString operator()() const
    {
      return {};
    }
    QString operator()(const State::impulse&) const
    {
      return {};
    }
    QString operator()(int i) const
    {
      const auto& loc = QLocale::c();
      auto str = loc.toString(i);
      str.remove(loc.groupSeparator());
      return str;
    }
    QString operator()(float f) const
    {
      const auto& loc = QLocale::c();
      auto str = loc.toString(f);
      str.remove(loc.groupSeparator());
      return str;
    }
    QString operator()(bool b) const
    {
      return b ? iscore::StringConstant().lowercase_true
               : iscore::StringConstant().lowercase_false;
    }
    QString operator()(const QString& s) const
    {
      // TODO escape ?
      return QString("\"%1\"").arg(s);
    }
    QString operator()(const std::string& s) const
    {
      return operator()(QString::fromStdString(s));
    }

    QString operator()(QChar c) const
    {
      return QString("'%1'").arg(c);
    }

    QString operator()(const vec2f& t) const
    {
      QString s{"["};

      s += this->operator()(t[0]);

      for (std::size_t i = 1; i < t.size(); i++)
      {
        s += ", ";
        s += this->operator()(t[i]);
      }

      s += "]";
      return s;
    }

    QString operator()(const vec3f& t) const
    {
      QString s{"["};

      s += this->operator()(t[0]);

      for (std::size_t i = 1; i < t.size(); i++)
      {
        s += ", ";
        s += this->operator()(t[i]);
      }

      s += "]";
      return s;
    }

    QString operator()(const vec4f& t) const
    {
      QString s{"["};

      s += this->operator()(t[0]);

      for (std::size_t i = 1; i < t.size(); i++)
      {
        s += ", ";
        s += this->operator()(t[i]);
      }

      s += "]";
      return s;
    }

    QString operator()(const State::tuple_t& t) const
    {
      QString s{"["};

      auto n = t.size();
      if (n >= 1)
      {
        s += ossia::apply(*this, t[0].v);
      }

      for (std::size_t i = 1; i < n; i++)
      {
        s += ", ";
        s += ossia::apply(*this, t[i].v);
      }

      s += "]";
      return s;
    }
  };

  return ossia::apply(vis{}, val.v);
}

namespace
{
struct convert_helper
{
  ossia::value& toConvert;
  void operator()() const
  {
    toConvert = ossia::value{};
  }
  void operator()(const State::impulse& v) const
  {
    toConvert = v;
  }

  template <typename T>
  void operator()(const T&) const
  {
    toConvert = value<T>(toConvert);
  }
};
}
bool convert(const ossia::value& orig, ossia::value& toConvert)
{
  ossia::apply(convert_helper{toConvert}, orig.v);
  return true;
}

static ossia::value fromQVariantImpl(const QVariant& val)
{
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wswitch-enum"
  switch (auto t = QMetaType::Type(val.type()))
  {
    case QMetaType::Int:
      return ossia::value{val.toInt()};
    case QMetaType::UInt:
      return ossia::value{(int)val.toUInt()};
    case QMetaType::Long:
      return ossia::value{(int)val.value<int64_t>()};
    case QMetaType::LongLong:
      return ossia::value{(int)val.toLongLong()};
    case QMetaType::ULong:
      return ossia::value{(int)val.value<uint64_t>()};
    case QMetaType::ULongLong:
      return ossia::value{(int)val.toULongLong()};
    case QMetaType::Short:
      return ossia::value{(int)val.value<int16_t>()};
    case QMetaType::UShort:
      return ossia::value{(int)val.value<uint16_t>()};
    case QMetaType::Float:
      return ossia::value{val.toFloat()};
    case QMetaType::Double:
      return ossia::value{(float)val.toDouble()};
    case QMetaType::Bool:
      return ossia::value{val.toBool()};
    case QMetaType::QString:
      return ossia::value{val.toString().toStdString()};
    case QMetaType::Char:
      return ossia::value{val.value<char>()};
    case QMetaType::QChar:
      return ossia::value{val.toChar().toLatin1()};
    case QMetaType::QVariantList:
    {
      auto list = val.value<QVariantList>();
      tuple_t tuple_val;
      tuple_val.reserve(list.size());

      Foreach(list, [&](const auto& elt) {
        tuple_val.push_back(fromQVariantImpl(elt));
      });
      return tuple_val;
    }
    case QMetaType::QVector2D:
    {
      auto vec = val.value<QVector2D>();
      return ossia::value{vec2f{{vec[0], vec[1]}}};
    }
    case QMetaType::QVector3D:
    {
      auto vec = val.value<QVector3D>();
      return ossia::value{vec3f{{vec[0], vec[1], vec[2]}}};
    }
    case QMetaType::QVector4D:
    {
      auto vec = val.value<QVector4D>();
      return ossia::value{vec4f{{vec[0], vec[1], vec[2], vec[3]}}};
    }
    default:
    {
      if (t == qMetaTypeId<ossia::value>())
      {
        return ossia::value{impulse{}};
      }
      else
      {
        return ossia::value{};
      }
    }
  }

#pragma GCC diagnostic warning "-Wswitch"
#pragma GCC diagnostic warning "-Wswitch-enum"
}

ossia::value fromQVariant(const QVariant& val)
{
  return ossia::value{fromQVariantImpl(val)};
}

QString prettyType(ossia::val_type t)
{
  return ValuePrettyTypes[static_cast<int>(t)];
}

const std::array<std::pair<QString, ossia::val_type>, 10>&
ValuePrettyTypesMap()
{
  return ValuePrettyTypesPairArray;
}

const std::array<const QString, 11>& ValuePrettyTypesArray()
{
  return ValuePrettyTypes;
}
}
}
