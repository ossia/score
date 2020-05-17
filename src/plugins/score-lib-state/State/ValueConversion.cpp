// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ValueConversion.hpp"

#include <State/Value.hpp>

#include <score/serialization/StringConstants.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/network/value/value.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <array>

namespace State
{
namespace convert
{
const std::array<const QString, 11> ValuePrettyTypes{
    {QObject::tr("Float"),
     QObject::tr("Int"),
     QObject::tr("Vec2f"),
     QObject::tr("Vec3f"),
     QObject::tr("Vec4f"),
     QObject::tr("Impulse"),
     QObject::tr("Bool"),
     QObject::tr("String"),
     QObject::tr("List"),
     QObject::tr("Char"),
     QObject::tr("Container")}};

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
     std::make_pair(QObject::tr("List"), ossia::val_type::LIST)}};

template <>
QVariant value(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QVariant;
    return_type operator()() const { return QVariant{}; }
    return_type operator()(const impulse&) const { return QVariant::fromValue(State::impulse{}); }
    return_type operator()(int i) const { return QVariant::fromValue(i); }
    return_type operator()(float f) const { return QVariant::fromValue(f); }
    return_type operator()(bool b) const { return QVariant::fromValue(b); }
    return_type operator()(const QString& s) const { return QVariant::fromValue(s); }
    return_type operator()(const std::string& s) const
    {
      return operator()(QString::fromStdString(s));
    }
    return_type operator()(char c) const { return QVariant::fromValue(QChar(c)); }
    return_type operator()(vec2f t) const { return QVector2D{t[0], t[1]}; }
    return_type operator()(vec3f t) const { return QVector3D{t[0], t[1], t[2]}; }
    return_type operator()(vec4f t) const { return QVector4D{t[0], t[1], t[2], t[3]}; }
    return_type operator()(const list_t& t) const
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

QString textualType(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QString;
    return_type operator()() const { return QStringLiteral("None"); }
    return_type operator()(impulse) const { return QStringLiteral("Impulse"); }
    return_type operator()(int i) const { return QStringLiteral("Int"); }
    return_type operator()(float f) const { return QStringLiteral("Float"); }
    return_type operator()(bool b) const { return QStringLiteral("Bool"); }
    return_type operator()(const std::string& s) const { return QStringLiteral("String"); }
    return_type operator()(char c) const { return QStringLiteral("Char"); }
    return_type operator()(vec2f t) const { return QStringLiteral("Vec2f"); }
    return_type operator()(vec3f t) const { return QStringLiteral("Vec3f"); }
    return_type operator()(vec4f t) const { return QStringLiteral("Vec4f"); }
    return_type operator()(const list_t& t) const { return QStringLiteral("List"); }
  };

  return ossia::apply(vis{}, val.v);
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
    return_type operator()() const { return 0; }
    return_type operator()(const impulse&) const { return 0; }
    return_type operator()(int v) const { return v; }
    return_type operator()(float v) const { return v; }
    return_type operator()(bool v) const { return v; }
    return_type operator()(const std::string& v) const
    {
      return QLocale::c().toInt(QString::fromStdString(v));
    }
    return_type operator()(char v) const { return QLocale::c().toInt(QString(v)); }
    return_type operator()(const vec2f& v) const { return 0; }
    return_type operator()(const vec3f& v) const { return 0; }
    return_type operator()(const vec4f& v) const { return 0; }
    return_type operator()(const list_t& v) const { return 0; }
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
    return_type operator()() const { return {}; }
    return_type operator()(const impulse&) const { return {}; }
    return_type operator()(int v) const { return v; }
    return_type operator()(float v) const { return v; }
    return_type operator()(bool v) const { return v; }
    return_type operator()(const QString& v) const { return QLocale::c().toFloat(v); }
    return_type operator()(const std::string& v) const
    {
      return operator()(QString::fromStdString(v));
    }
    return_type operator()(char v) const { return QLocale::c().toFloat(QString(v)); }
    return_type operator()(const vec2f& v) const { return 0; }
    return_type operator()(const vec3f& v) const { return 0; }
    return_type operator()(const vec4f& v) const { return 0; }
    return_type operator()(const list_t& v) const { return {}; }
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
    return_type operator()() const { return {}; }
    return_type operator()(const impulse&) const { return {}; }
    return_type operator()(int v) const { return v; }
    return_type operator()(float v) const { return v; }
    return_type operator()(bool v) const { return v; }
    return_type operator()(const std::string& ve) const
    {
      auto& strings = score::StringConstant();

      auto& v = ve;
      return v == strings.lowercase_true || v == strings.True || v == strings.lowercase_yes
             || v == strings.Yes;
    }
    return_type operator()(char v) const { return v == 't' || v == 'T' || v == 'y' || v == 'Y'; }
    return_type operator()(const vec2f& v) const { return false; }
    return_type operator()(const vec3f& v) const { return false; }
    return_type operator()(const vec4f& v) const { return false; }
    return_type operator()(const list_t& v) const { return false; }
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
    return_type operator()() const { return '-'; }
    return_type operator()(const impulse&) const { return '-'; }
    return_type operator()(int) const { return '-'; }
    return_type operator()(float) const { return '-'; }
    return_type operator()(bool v) const { return v ? 'T' : 'F'; }
    return_type operator()(const std::string& s) const
    {
      return !s.empty() ? s[0] : '-';
    } // TODO boueeeff
    return_type operator()(char v) const { return v; }
    return_type operator()(const vec2f& v) const { return '-'; }
    return_type operator()(const vec3f& v) const { return '-'; }
    return_type operator()(const vec4f& v) const { return '-'; }
    return_type operator()(const list_t&) const { return '-'; }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template <>
QString value(const ossia::value& val)
{
  struct
  {
    using return_type = QString;
    return_type operator()() const { return {}; }
    return_type operator()(const State::impulse&) const { return {}; }
    return_type operator()(int i) const { return QLocale::c().toString(i); }
    return_type operator()(float f) const { return QLocale::c().toString(f); }
    return_type operator()(bool b) const
    {
      return QString::fromStdString(
          b ? score::StringConstant().lowercase_true : score::StringConstant().lowercase_false);
    }
    return_type operator()(const std::string& s) const { return QString::fromStdString(s); }
    return_type operator()(char c) const { return QChar(c); }
    return_type operator()(const vec2f& v) const { return {}; }
    return_type operator()(const vec3f& v) const { return {}; }
    return_type operator()(const vec4f& v) const { return {}; }
    return_type operator()(const std::vector<ossia::value>& t) const { return {}; }
  } visitor{};

  return ossia::apply(visitor, val.v);
}

template <std::size_t N>
auto string_to_vec(std::string_view s0)
{
  std::array<float, N> o{};
  if (!s0.empty() && s0.front() == '[')
    s0.remove_prefix(1);
  if (!s0.empty() && s0.back() == ']')
    s0.remove_suffix(1);

  // todo check when boost updates
  // to have this work without malloc
  std::string s{s0};
  using tok_t = boost::tokenizer<boost::escaped_list_separator<char>>;

  tok_t tok(s);
  std::size_t i = 0;
  for (std::string_view f : tok)
  {
    f.remove_prefix(std::min(f.find_first_not_of(" "), f.size()));

    if (auto trim_pos = f.find(' '); trim_pos != f.npos)
      f.remove_suffix(f.size() - trim_pos);

    if (!boost::conversion::detail::try_lexical_convert(f, o[i]))
      o[i] = 0.f;

    i++;
    if (i >= N)
      break;
  }
  return o;
}

template <>
vec2f value(const ossia::value& val)
{
  struct vis
  {
    using return_type = vec2f;
    return_type operator()() const { return {}; }
    return_type operator()(const State::impulse&) const { return {}; }
    return_type operator()(int i) const { return {{float(i)}}; }
    return_type operator()(float f) const { return {{f}}; }
    return_type operator()(bool b) const { return {{float(b)}}; }
    return_type operator()(const std::string& s) const { return string_to_vec<2>(s); }
    return_type operator()(char c) const { return {}; }
    return_type operator()(const vec2f& v) const { return v; }
    return_type operator()(const vec3f& v) const { return {{v[0], v[1]}}; }
    return_type operator()(const vec4f& v) const { return {{v[0], v[1]}}; }
    return_type operator()(const std::vector<ossia::value>& t) const
    {
      const std::size_t n = t.size();
      const std::size_t n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (std::size_t i = 0; i < std::min(n, n_2); i++)
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
    return_type operator()() const { return {}; }
    return_type operator()(const State::impulse&) const { return {}; }
    return_type operator()(int i) const { return {{float(i)}}; }
    return_type operator()(float f) const { return {{f}}; }
    return_type operator()(bool b) const { return {{float(b)}}; }

    return_type operator()(const std::string& s) const { return string_to_vec<3>(s); }
    return_type operator()(char c) const { return {}; }
    return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
    return_type operator()(const vec3f& v) const { return v; }
    return_type operator()(const vec4f& v) const { return {{v[0], v[1], v[2]}}; }
    return_type operator()(const std::vector<ossia::value>& t) const
    {
      const std::size_t n = t.size();
      const std::size_t n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (std::size_t i = 0; i < std::min(n, n_2); i++)
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
    return_type operator()() const { return {}; }
    return_type operator()(const State::impulse&) const { return {}; }
    return_type operator()(int i) const { return {{float(i)}}; }
    return_type operator()(float f) const { return {{f}}; }
    return_type operator()(bool b) const { return {{float(b)}}; }
    return_type operator()(const std::string& s) const { return string_to_vec<4>(s); }
    return_type operator()(char c) const { return {}; }
    return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
    return_type operator()(const vec3f& v) const { return {{v[0], v[1], v[2]}}; }
    return_type operator()(const vec4f& v) const { return v; }
    return_type operator()(const std::vector<ossia::value>& t) const
    {
      const std::size_t n = t.size();
      const std::size_t n_2 = std::tuple_size<return_type>::value;
      return_type v{};
      for (std::size_t i = 0; i < std::min(n, n_2); i++)
      {
        v[i] = value<float>(t[i]);
      }
      return v;
    }
  };

  return ossia::apply(vis{}, val.v);
}

template <>
list_t value(const ossia::value& val)
{
  struct vis
  {
    using return_type = list_t;
    return_type operator()() const { return {}; }
    return_type operator()(const State::impulse&) const { return {impulse{}}; }
    return_type operator()(int i) const { return {i}; }
    return_type operator()(float f) const { return {f}; }
    return_type operator()(bool b) const { return {b}; }
    return_type operator()(const std::string& s) const
    {
      auto v = parseValue(s);

      if (v)
      {
        if (auto t = v->target<list_t>())
          return *t;
      }

      return {s};
    }
    return_type operator()(char c) const { return {}; }
    return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
    return_type operator()(const vec3f& v) const { return {{v[0], v[1], v[2]}}; }
    return_type operator()(const vec4f& v) const { return {{v[0], v[1], v[2], v[3]}}; }
    return_type operator()(const std::vector<ossia::value>& t) const { return t; }
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
    QString operator()() const { return {}; }
    QString operator()(const State::impulse&) const { return {}; }
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
      return QString::fromStdString(
          b ? score::StringConstant().lowercase_true : score::StringConstant().lowercase_false);
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

    QString operator()(const std::vector<ossia::value>& t) const
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
  void operator()() const { toConvert = ossia::value{}; }
  void operator()(const State::impulse& v) const { toConvert = v; }

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

ossia::value fromQVariant(const QVariant& val)
{
  return ossia::qt::qt_to_ossia{}(val);
}

QString prettyType(ossia::val_type t)
{
  return ValuePrettyTypes[static_cast<int>(t)];
}

const std::array<std::pair<QString, ossia::val_type>, 10>& ValuePrettyTypesMap()
{
  return ValuePrettyTypesPairArray;
}

const std::array<const QString, 11>& ValuePrettyTypesArray()
{
  return ValuePrettyTypes;
}
}
}
