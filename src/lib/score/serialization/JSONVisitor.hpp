#pragma once
#include <score/model/EntityBase.hpp>
#include <score/serialization/CommonTypes.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/serialization/VisitorTags.hpp>
#include <score/tools/ForEach.hpp>

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/json.hpp>
#include <ossia/detail/small_vector.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include <QDebug>

/**
 * This file contains facilities
 * to serialize an object into a QJsonObject.
 */

namespace score
{
class ApplicationComponents;
}
using JsonStream = rapidjson::Writer<rapidjson::StringBuffer>;
struct OptionalSentinel
{
  template <typename T>
  friend bool operator==(std::optional<T>& lhs, const OptionalSentinel& rhs)
  {
    return !bool(lhs);
  }

  template <typename T>
  friend bool operator!=(std::optional<T>& lhs, const OptionalSentinel& rhs)
  {
    return bool(lhs);
  }
};

class SCORE_LIB_BASE_EXPORT JSONReader : public AbstractVisitor
{
public:
  using type = JSONObject;
  using is_visitor_tag = std::integral_constant<bool, true>;

  JSONReader();
  JSONReader(const JSONReader&) = delete;
  JSONReader& operator=(const JSONReader&) = delete;
  JSONReader(JSONReader&&) = default;
  JSONReader& operator=(JSONReader&&) = delete;

  VisitorVariant toVariant() { return {*this, JSONObject::type()}; }

  template <typename T>
  static auto marshall(const T& t)
  {
    JSONReader reader;
    reader.readFrom(t);
    return reader;
  }

  bool empty() const noexcept { return this->buffer.GetLength() == 0; }

  void read(const QString&) const noexcept = delete;
  void read(const float&) const noexcept = delete;
  void read(const char&) const noexcept = delete;
  void read(const int&) const noexcept = delete;
  void read(const bool&) const noexcept = delete;
  void read(const std::string&) const noexcept = delete;
  void read(const unsigned int&) const noexcept = delete;
  void read(const unsigned char&) const noexcept = delete;

  //! Called by code that wants to serialize.
  template <typename T>
  void readFrom(const score::Entity<T>& obj)
  {
    TSerializer<JSONObject, score::Entity<T>>::readFrom(*this, obj);
  }

  template <typename T>
  void readFrom(const IdentifiedObject<T>& obj)
  {
    TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(*this, obj);
  }

  void readFrom(const QString& obj) noexcept { readFrom(obj.toUtf8()); }
  void readFrom(const QByteArray& t) noexcept { stream.String(t.data(), t.size()); }
  void readFrom(const std::string& t) noexcept { stream.String(t.data(), t.size()); }
  void readFrom(int64_t t) noexcept { stream.Int64(t); }
  void readFrom(int32_t t) noexcept { stream.Int(t); }
  void readFrom(uint64_t t) noexcept { stream.Uint64(t); }
  void readFrom(uint32_t t) noexcept { stream.Uint(t); }
  void readFrom(float t) noexcept { stream.Double(t); }
  void readFrom(double t) noexcept { stream.Double(t); }
  void readFrom(bool t) noexcept { stream.Bool(t); }
  void readFrom(char t) noexcept { stream.String(&t, 1); }
  void readFrom(const void*) noexcept = delete;

  template <typename T>
  void readFrom(const T& obj)
  {
    static constexpr bool has_base = base_kind<T>::value;

    if constexpr (has_base)
    {
      readFrom((const typename T::base_type&)obj);
    }
    else if constexpr (std::is_enum_v<T>)
    {
      check_enum_size<T> _;
      Q_UNUSED(_);
      stream.Int(static_cast<int32_t>(obj));
    }
    else
    {
      if constexpr ((is_template<T>::value && !is_abstract_base<T>::value && !is_identified_object<T>::value) || is_custom_serialized<T>::value)
      {
        TSerializer<JSONObject, T>::readFrom(*this, obj);
      }
      else if constexpr (is_identified_object<T>::value && !is_entity<T>::value && !is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        stream.StartObject();
        TSerializer<JSONObject, typename T::object_type>::readFrom(*this, obj);

        if constexpr (is_custom_serialized<T>::value || is_template<T>::value)
          TSerializer<JSONObject, T>::readFrom(*this, obj);
        else
          read(obj);

        stream.EndObject();
      }
      else if constexpr (is_entity<T>::value && !is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        stream.StartObject();
        TSerializer<JSONObject, typename T::entity_type>::readFrom(*this, obj);

        if constexpr (is_custom_serialized<T>::value || is_template<T>::value)
          TSerializer<JSONObject, T>::readFrom(*this, obj);
        else
          read(obj);
        stream.EndObject();
      }
      else if constexpr (!is_identified_object<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        stream.StartObject();
        readFromAbstract(obj, [](JSONReader& sub, const T& obj) {
          // Read the implementation of the base object
          sub.read(obj);
        });
        stream.EndObject();
      }
      else if constexpr (is_identified_object<T>::value && !is_entity<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        stream.StartObject();
        readFromAbstract(obj, [](JSONReader& sub, const T& obj) {
          TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(sub, obj);

          if constexpr (is_custom_serialized<T>::value || is_template<T>::value)
            TSerializer<JSONObject, T>::readFrom(sub, obj);
          else
            sub.read(obj);
        });
        stream.EndObject();
      }
      else if constexpr (is_entity<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        stream.StartObject();
        readFromAbstract(obj, [](JSONReader& sub, const T& obj) {
          TSerializer<JSONObject, score::Entity<T>>::readFrom(sub, obj);

          if constexpr (is_custom_serialized<T>::value || is_template<T>::value)
            TSerializer<JSONObject, T>::readFrom(sub, obj);
          else
            sub.read(obj);
        });
        stream.EndObject();
      }
      else
      {
        //! Used to serialize general objects that won't fit in the other
        //! categories
        read(obj);
      }
    }
  }

  rapidjson::StringBuffer buffer;
  JsonStream stream{buffer};
  struct assigner;
  struct fake_obj
  {
    JSONReader& self;
    assigner operator[](std::string_view str) const noexcept;
    template <std::size_t N>
    assigner operator[](const char (&str)[N]) const noexcept;
    assigner operator[](const QString& str) const noexcept;
  } obj;

  const score::ApplicationComponents& components;
  const score::StringConstants& strings;

  QByteArray toByteArray() const
  {
    SCORE_ASSERT(stream.IsComplete());
    return QByteArray{buffer.GetString(), (int)buffer.GetLength()};
  }
  std::string toStdString() const
  {
    SCORE_ASSERT(stream.IsComplete());
    return std::string{buffer.GetString(), buffer.GetLength()};
  }
  QString toString() const
  {
    SCORE_ASSERT(stream.IsComplete());
    return QString::fromUtf8(buffer.GetString(), buffer.GetLength());
  }

  //! Serializable types should reimplement this method
  //! It is not to be called by user code.
  template <typename T>
  void read(const T&);

private:
  template <typename T, typename Fun>
  void readFromAbstract(const T& in, Fun f);
};

struct JSONReader::assigner
{
  JSONReader& self;

  void operator=(int64_t t) const noexcept { self.stream.Int64(t); }
  void operator=(int32_t t) const noexcept { self.stream.Int(t); }
  void operator=(uint64_t t) const noexcept { self.stream.Uint64(t); }
  void operator=(uint32_t t) const noexcept { self.stream.Uint(t); }
  void operator=(float t) const noexcept { self.stream.Double(t); }
  void operator=(double t) const noexcept { self.stream.Double(t); }
  void operator=(bool t) const noexcept { self.stream.Bool(t); }
  void operator=(char t) const noexcept { self.stream.String(&t, 1); }
  void operator=(QPoint t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Int(t.x());
    self.stream.Int(t.y());
    self.stream.EndArray();
  }
  void operator=(QPointF t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Double(t.x());
    self.stream.Double(t.y());
    self.stream.EndArray();
  }
  void operator=(QSize t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Int(t.width());
    self.stream.Int(t.height());
    self.stream.EndArray();
  }
  void operator=(QSizeF t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Double(t.width());
    self.stream.Double(t.height());
    self.stream.EndArray();
  }
  void operator=(QRect t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Int(t.x());
    self.stream.Int(t.y());
    self.stream.Int(t.width());
    self.stream.Int(t.height());
    self.stream.EndArray();
  }
  void operator=(QRectF t) const noexcept
  {
    self.stream.StartArray();
    self.stream.Double(t.x());
    self.stream.Double(t.y());
    self.stream.Double(t.width());
    self.stream.Double(t.height());
    self.stream.EndArray();
  }
  void operator=(const QString& t) const noexcept { *this = t.toUtf8(); }
  void operator=(const QStringList& t) const noexcept
  {
    self.stream.StartArray();
    for (const auto& str : t)
      *this = str.toUtf8();
    self.stream.EndArray();
  }
  void operator=(const QLatin1String& t) const noexcept { self.stream.String(t.data(), t.size()); }
  void operator=(const std::string& t) const noexcept { self.stream.String(t.data(), t.length()); }
  void operator=(const std::string_view& t) const noexcept
  {
    self.stream.String(t.data(), t.length());
  }
  void operator=(const QByteArray& t) const noexcept { self.stream.String(t.data(), t.length()); }
  void operator=(const QVariantMap& t) const noexcept { SCORE_ABORT; }

  template <typename T>
  void operator=(const T& t) const noexcept
  {
    if constexpr (std::is_enum_v<T>)
    {
      check_enum_size<T> _;
      Q_UNUSED(_);
      self.stream.Int(static_cast<int32_t>(t));
    }
    else
    {
      self.readFrom(t);
    }
  }
};

inline JSONReader::assigner JSONReader::fake_obj::operator[](std::string_view str) const noexcept
{
  self.stream.Key(str.data(), str.length());
  return assigner{self};
}
inline JSONReader::assigner JSONReader::fake_obj::operator[](const QString& str) const noexcept
{
  const std::string& s = str.toStdString();
  self.stream.Key(s.data(), s.length());
  return assigner{self};
}
template <std::size_t N>
inline JSONReader::assigner JSONReader::fake_obj::operator[](const char (&str)[N]) const noexcept
{
  return (*this)[std::string_view(str, N - 1)];
}

template <typename T, typename Fun>
void JSONReader::readFromAbstract(const T& in, Fun f)
{
  obj[strings.uuid] = in.concreteKey().impl();
  f(*this, in);
  in.serialize_impl(this->toVariant());
}

struct JsonValue
{
  const rapidjson::Value& obj;
  QString toString() const noexcept
  {
    return QString::fromUtf8(obj.GetString(), obj.GetStringLength());
  }
  std::string toStdString() const noexcept
  {
    return std::string(obj.GetString(), obj.GetStringLength());
  }
  QByteArray toByteArray() const noexcept
  {
    return QByteArray(obj.GetString(), obj.GetStringLength());
  }

  int32_t toInt() const noexcept { return obj.GetInt(); }
  bool toBool() const noexcept { return obj.GetBool(); }
  double toDouble() const noexcept { return obj.GetDouble(); }
  bool isDouble() const noexcept { return obj.IsDouble(); }
  auto toArray() const noexcept { return obj.GetArray(); }
  auto toObject() const noexcept { return obj.GetObject(); }
  bool isString() const noexcept { return obj.IsString(); }

  template <std::size_t N>
  JsonValue operator[](const char (&str)[N]) const noexcept
  {
    return JsonValue{obj[str]};
  }
  JsonValue operator[](const std::string& str) const noexcept { return JsonValue{obj[str]}; }
  JsonValue operator[](const QString& str) const noexcept { return (*this)[str.toStdString()]; }
  template <typename T>
  friend void operator<<=(T& t, const JsonValue& self);

  template <typename T>
  T to() const noexcept
  {
    T t;
    t <<= *this;
    return t;
  }
};

class SCORE_LIB_BASE_EXPORT JSONWriter : public AbstractVisitor
{
public:
  using type = JSONObject;
  using is_visitor_tag = std::integral_constant<bool, true>;
  using is_deserializer_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant() { return {*this, JSONObject::type()}; }

  JSONWriter() = delete;
  JSONWriter(const JSONWriter&) = delete;
  JSONWriter& operator=(const JSONWriter&) = delete;

  explicit JSONWriter(const rapidjson::Value& obj);
  explicit JSONWriter(const JsonValue& obj);

  template <typename T>
  static auto unmarshall(const rapidjson::Value& obj)
  {
    T data;
    JSONWriter wrt{obj};
    wrt.writeTo(data);
    return data;
  }

  template <typename T>
  void write(T&);

  void write(QString&) = delete;
  void write(float&) = delete;
  void write(char&) = delete;
  void write(int&) = delete;
  void write(bool&) = delete;
  void write(std::string&) = delete;

  template <typename T>
  void writeTo(T& obj)
  {
    if constexpr ((is_template<T>::value && !is_abstract_base<T>::value && !is_identified_object<T>::value) || is_custom_serialized<T>::value)
    {
      TSerializer<JSONObject, T>::writeTo(*this, obj);
    }
    else if constexpr (std::is_enum<T>::value)
    {
      obj = static_cast<T>(base.GetInt());
    }
    else
    {
      write(obj);
    }
  }

  const rapidjson::Value& base;

  struct wrapper
  {
    const rapidjson::Value& ref;
    template <std::size_t N>
    JsonValue operator[](const char (&str)[N]) const noexcept
    {
      return JsonValue{ref[str]};
    }
    JsonValue operator[](const std::string& str) const noexcept { return JsonValue{ref[str]}; }
    JsonValue operator[](const QString& str) const noexcept { return (*this)[str.toStdString()]; }
    template <std::size_t N>
    std::optional<JsonValue> tryGet(const char (&str)[N]) const noexcept
    {
      if (auto it = ref.FindMember(str); it != ref.MemberEnd())
        return JsonValue{it->value};
      return std::nullopt;
    }
    std::optional<JsonValue> tryGet(const std::string& str) const noexcept
    {
      if (auto it = ref.FindMember(str); it != ref.MemberEnd())
        return JsonValue{it->value};
      return std::nullopt;
    }
    std::optional<JsonValue> tryGet(const QString& str) const noexcept
    {
      return tryGet(str.toStdString());
    }
    std::optional<JsonValue> constFind(const std::string& str) const noexcept { return tryGet(str); }
    std::optional<JsonValue> constFind(const QString& str) const noexcept
    {
      return tryGet(str.toStdString());
    }
    auto constEnd() const noexcept { return OptionalSentinel{}; }
  } obj{base};

  const score::ApplicationComponents& components;
  const score::StringConstants& strings;
};

template <typename T>
inline void operator<<=(T& t, const JsonValue& self)
{
  JSONWriter w{self.obj};
  w.writeTo(t);
}

inline void operator<<=(QString& t, const JsonValue& self)
{
  t = self.toString();
}
inline void operator<<=(float& t, const JsonValue& self)
{
  t = self.obj.GetFloat();
}
inline void operator<<=(double& t, const JsonValue& self)
{
  t = self.obj.GetDouble();
}
inline void operator<<=(int& t, const JsonValue& self)
{
  t = self.obj.GetInt();
}
inline void operator<<=(int64_t& t, const JsonValue& self)
{
  t = self.obj.GetInt();
}
inline void operator<<=(std::string& t, const JsonValue& self)
{
  t = self.toStdString();
}
inline void operator<<=(QByteArray& t, const JsonValue& self)
{
  t = self.toByteArray();
}
inline void operator<<=(bool& t, const JsonValue& self)
{
  t = self.toBool();
}
inline void operator<<=(char& t, const JsonValue& self)
{
  t = self.obj.GetStringLength() > 0 ? self.obj.GetString()[0] : '\0';
}

template <typename T>
struct TSerializer<JSONObject, IdentifiedObject<T>>
{
  template <typename U>
  static void readFrom(JSONObject::Serializer& s, const IdentifiedObject<U>& obj)
  {
    s.obj[s.strings.ObjectName] = obj.objectName();
    s.obj[s.strings.id] = obj.id().val();
  }

  template <typename U>
  static void writeTo(JSONObject::Deserializer& s, IdentifiedObject<U>& obj)
  {
    obj.setObjectName(s.obj[s.strings.ObjectName].toString());
    obj.setId(Id<T>{s.obj[s.strings.id].toInt()});
  }
};

Q_DECLARE_METATYPE(JSONReader*)
Q_DECLARE_METATYPE(JSONWriter*)
W_REGISTER_ARGTYPE(JSONReader*)
W_REGISTER_ARGTYPE(JSONWriter*)

struct ArraySerializer
{
  template <typename T>
  static void readFrom(JSONObject::Serializer& s, const T& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.readFrom(elt);
    s.stream.EndArray();
  }

  template <typename T>
  static void writeTo(JSONObject::Deserializer& s, T& vec)
  {
    const auto& array = s.base.GetArray();

    vec.clear();
    vec.reserve(array.Size());
    for (const auto& elt : array)
    {
      typename T::value_type v;
      JSONObject::Deserializer des{elt};
      des.writeTo(v);
      vec.push_back(std::move(v));
    }
  }

  template <typename Arg, std::size_t N>
  static void readFrom(JSONObject::Serializer& s, const std::array<Arg, N>& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.readFrom(elt);
    s.stream.EndArray();
  }

  template <typename Arg, std::size_t N>
  static void writeTo(JSONObject::Deserializer& s, std::array<Arg, N>& vec)
  {
    const auto& array = s.base.GetArray();
    SCORE_ASSERT(N >= array.Size());

    auto it = vec.begin();
    for (const auto& elt : array)
    {
      JSONObject::Deserializer des{elt};
      des.writeTo(*it);
      ++it;
    }
  }

  template <std::size_t N>
  static void readFrom(JSONObject::Serializer& s, const std::array<float, N>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Double(elt);
    s.stream.EndArray();
  }

  template <std::size_t N>
  static void writeTo(JSONObject::Deserializer& s, std::array<float, N>& vec)
  {
    const auto& array = s.base.GetArray();
    SCORE_ASSERT(N >= array.Size());

    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetFloat();
      ++it;
    }
  }

  template <typename T>
  static void readFrom(JSONObject::Serializer& s, const std::list<T>& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.readFrom(elt);
    s.stream.EndArray();
  }

  template <typename T>
  static void writeTo(JSONObject::Deserializer& s, std::list<T>& vec)
  {
    vec.clear();

    const auto& array = s.base.GetArray();
    for (const auto& elt : array)
    {
      T v;
      JSONObject::Deserializer des{elt};
      des.writeTo(v);
      vec.push_back(std::move(v));
    }
  }

  // REMOVEME
  template <typename T>
  static void readFrom(JSONObject::Serializer& s, const QList<T>& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.readFrom(elt);
    s.stream.EndArray();
  }

  template <typename T>
  static void writeTo(JSONObject::Deserializer& s, QList<T>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.reserve(array.Size());

    for (const auto& elt : array)
    {
      T v;
      JSONObject::Deserializer des{elt};
      des.writeTo(v);
      vec.push_back(std::move(v));
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<std::string, Args...>& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.stream.String(elt.data(), elt.size());
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<std::string, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.reserve(array.Size());
    for (const auto& elt : array)
    {
      vec.push_back(std::string{elt.GetString(), elt.GetStringLength()});
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<QString, Args...>& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
    {
      const QByteArray& b = elt.toUtf8();
      s.stream.String(b.data(), b.size());
    }
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<QString, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.reserve(array.Size());
    for (const auto& elt : array)
    {
      vec.push_back(QString::fromUtf8(elt.GetString(), elt.GetStringLength()));
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<int, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Int(elt);
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<int, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetInt();
      ++it;
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<char, Args...>& vec)
  {
    s.stream.StartArray();
    for (char elt : vec)
      s.stream.String(&elt, 1);
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<char, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetString()[0];
      ++it;
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<int64_t, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Int64(elt);
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<int64_t, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetInt64();
      ++it;
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<float, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Double(elt);
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<float, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetFloat();
      ++it;
    }
  }

  template <template <typename... Args> typename T, typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<double, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Double(elt);
    s.stream.EndArray();
  }

  template <template <typename... Args> typename T, typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<double, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetDouble();
      ++it;
    }
  }

  // Overloads for supporting static/small vectors... remove when we have
  // concepts
  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<int, N, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Int(elt);
    s.stream.EndArray();
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<int, N, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetInt();
      ++it;
    }
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<char, N, Args...>& vec)
  {
    s.stream.StartArray();
    for (char elt : vec)
      s.stream.String(&elt, 1);
    s.stream.EndArray();
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<char, N, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetString()[0];
      ++it;
    }
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<int64_t, N, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Int64(elt);
    s.stream.EndArray();
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<int64_t, N, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetInt64();
      ++it;
    }
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<float, N, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Double(elt);
    s.stream.EndArray();
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<float, N, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetFloat();
      ++it;
    }
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void readFrom(JSONObject::Serializer& s, const T<double, N, Args...>& vec)
  {
    s.stream.StartArray();
    for (auto elt : vec)
      s.stream.Double(elt);
    s.stream.EndArray();
  }

  template <
      template <typename, std::size_t, typename...>
      typename T,
      std::size_t N,
      typename... Args>
  static void writeTo(JSONObject::Deserializer& s, T<double, N, Args...>& vec)
  {
    const auto& array = s.base.GetArray();
    vec.clear();
    vec.resize(array.Size());
    auto it = vec.begin();
    for (const auto& elt : array)
    {
      *it = elt.GetDouble();
      ++it;
    }
  }
};

template <typename... Args>
struct TSerializer<JSONObject, std::vector<Args...>> : ArraySerializer
{
};

template <typename... Args>
struct TSerializer<JSONObject, std::list<Args...>> : ArraySerializer
{
};

template <typename... Args>
struct TSerializer<JSONObject, QList<Args...>> : ArraySerializer
{
};

template <typename T, std::size_t N, typename Alloc>
struct TSerializer<JSONObject, boost::container::small_vector<T, N, Alloc>> : ArraySerializer
{
};

template <typename T, std::size_t N>
struct TSerializer<JSONObject, boost::container::static_vector<T, N>> : ArraySerializer
{
};

template <typename T, std::size_t N>
struct TSerializer<JSONObject, std::array<T, N>> : ArraySerializer
{
};

template <std::size_t N>
struct TSerializer<JSONObject, std::array<float, N>> : ArraySerializer
{
};

template <typename T, typename U>
struct TSerializer<JSONObject, IdContainer<T, U>> : ArraySerializer
{
};

template <typename T>
struct TSerializer<JSONObject, std::optional<T>>
{
  static void readFrom(JSONObject::Serializer& s, const std::optional<T>& obj)
  {
    if (obj)
      s.readFrom(*obj);
    else
      s.stream.Null();
  }

  static void writeTo(JSONObject::Deserializer& s, std::optional<T>& obj)
  {
    if (s.base.IsNull())
    {
      obj = std::nullopt;
    }
    else
    {
      T t;
      t <<= JsonValue{s.base};
      obj = std::move(t);
    }
  }
};

template <typename T, typename U>
struct TSerializer<JSONObject, std::pair<T, U>>
{
  using type = std::pair<T, U>;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    s.stream.StartArray();
    s.readFrom(obj.first);
    s.readFrom(obj.second);
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    const auto& arr = s.base.GetArray();
    obj.first <<= JsonValue{arr[0]};
    obj.second <<= JsonValue{arr[1]};
  }
};

template <>
struct TSerializer<JSONObject, QVariantMap>
{
  using type = QVariantMap;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    SCORE_ABORT;
    /*
    QJsonArray arr;
    arr.append(toJsonValue(obj.first));
    arr.append(toJsonValue(obj.second));
    s.val = std::move(arr);
    */
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    SCORE_ABORT;
    /*
    const auto arr = s.val.toArray();
    obj.first = fromJsonValue<T>(arr[0]);
    obj.second = fromJsonValue<U>(arr[1]);
    */
  }
};

template <typename T>
struct TSerializer<JSONObject, ossia::flat_set<T>>
{
  using type = ossia::flat_set<T>;
  static void readFrom(JSONObject::Serializer& s, const type& obj)
  {
    ArraySerializer::readFrom(s, obj.container);
  }

  static void writeTo(JSONObject::Deserializer& s, type& obj)
  {
    ArraySerializer::writeTo(s, obj.container);
  }
};

template <>
struct TSerializer<JSONObject, QColor>
{
  static void readFrom(JSONObject::Serializer& s, QColor c)
  {
    const auto col = c.rgba64();
    s.stream.StartArray();
    s.stream.Int(col.red());
    s.stream.Int(col.green());
    s.stream.Int(col.blue());
    s.stream.Int(col.alpha());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QColor& c)
  {
    const auto& array = s.base.GetArray();
    QRgba64 col;
    col.setRed(array[0].GetInt());
    col.setGreen(array[1].GetInt());
    col.setBlue(array[2].GetInt());
    col.setAlpha(array[3].GetInt());
    c = col;
  }
};

template <>
struct TSerializer<JSONObject, QPoint>
{
  static void readFrom(JSONObject::Serializer& s, QPoint c)
  {
    s.stream.StartArray();
    s.stream.Int(c.x());
    s.stream.Int(c.y());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QPoint& c)
  {
    const auto& array = s.base.GetArray();
    c.setX(array[0].GetInt());
    c.setY(array[1].GetInt());
  }
};

template <>
struct TSerializer<JSONObject, QPointF>
{
  static void readFrom(JSONObject::Serializer& s, QPointF c)
  {
    s.stream.StartArray();
    s.stream.Double(c.x());
    s.stream.Double(c.y());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QPointF& c)
  {
    const auto& array = s.base.GetArray();
    c.setX(array[0].GetDouble());
    c.setY(array[1].GetDouble());
  }
};

template <>
struct TSerializer<JSONObject, QSize>
{
  static void readFrom(JSONObject::Serializer& s, QSize c)
  {
    s.stream.StartArray();
    s.stream.Int(c.width());
    s.stream.Int(c.height());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QSize& c)
  {
    const auto& array = s.base.GetArray();
    c.setWidth(array[0].GetInt());
    c.setHeight(array[1].GetInt());
  }
};

template <>
struct TSerializer<JSONObject, QSizeF>
{
  static void readFrom(JSONObject::Serializer& s, QSizeF c)
  {
    s.stream.StartArray();
    s.stream.Double(c.width());
    s.stream.Double(c.height());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QSizeF& c)
  {
    const auto& array = s.base.GetArray();
    c.setWidth(array[0].GetDouble());
    c.setHeight(array[1].GetDouble());
  }
};

template <>
struct TSerializer<JSONObject, QRect>
{
  static void readFrom(JSONObject::Serializer& s, QRect c)
  {
    s.stream.StartArray();
    s.stream.Int(c.x());
    s.stream.Int(c.y());
    s.stream.Int(c.width());
    s.stream.Int(c.height());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QRect& c)
  {
    const auto& array = s.base.GetArray();
    c.setX(array[0].GetInt());
    c.setY(array[1].GetInt());
    c.setWidth(array[2].GetInt());
    c.setHeight(array[3].GetInt());
  }
};

template <>
struct TSerializer<JSONObject, QRectF>
{
  static void readFrom(JSONObject::Serializer& s, QRectF c)
  {
    s.stream.StartArray();
    s.stream.Double(c.x());
    s.stream.Double(c.y());
    s.stream.Double(c.width());
    s.stream.Double(c.height());
    s.stream.EndArray();
  }

  static void writeTo(JSONObject::Deserializer& s, QRectF& c)
  {
    const auto& array = s.base.GetArray();
    c.setX(array[0].GetDouble());
    c.setY(array[1].GetDouble());
    c.setWidth(array[2].GetDouble());
    c.setHeight(array[3].GetDouble());
  }
};
template <typename T>
struct TSerializer<JSONObject, Id<T>>
{
  using type = Id<T>;
  static void readFrom(JSONObject::Serializer& s, const type& obj) { s.stream.Int64(obj.val()); }

  static void writeTo(JSONObject::Deserializer& s, type& obj) { obj.setVal(s.base.GetInt64()); }
};

template <>
struct SCORE_LIB_BASE_EXPORT TSerializer<DataStream, rapidjson::Document>
{
  static void readFrom(DataStream::Serializer& s, const rapidjson::Document& obj);
  static void writeTo(DataStream::Deserializer& s, rapidjson::Document& obj);
};
template <>
struct SCORE_LIB_BASE_EXPORT TSerializer<DataStream, rapidjson::Value>
{
  static void readFrom(DataStream::Serializer& s, rapidjson::Value& obj) = delete;
  static void writeTo(DataStream::Deserializer& s, rapidjson::Value& obj) = delete;
};

namespace Process
{
class Inlet;
}
template <>
void JSONReader::read<Process::Inlet*>(Process::Inlet* const&) = delete;

SCORE_LIB_BASE_EXPORT
rapidjson::Document clone(const rapidjson::Value& val) noexcept;

inline rapidjson::Document readJson(const QByteArray& arr)
{
  rapidjson::Document doc;
  doc.Parse(arr.data(), arr.size());
  if (doc.HasParseError())
  {
    qDebug() << "Invalid JSON document !";
  }
  return doc;
}

inline QByteArray jsonToByteArray(const rapidjson::Value& arr) noexcept
{
  rapidjson::StringBuffer buf;
  buf.Reserve(8192);
  rapidjson::Writer<rapidjson::StringBuffer> w{buf};
  arr.Accept(w);
  return QByteArray(buf.GetString(), buf.GetSize());
}

rapidjson::Document toValue(const JSONReader&) noexcept;

template <typename T>
T fromJson(const QByteArray& rawData)
{
  const rapidjson::Document doc = readJson(rawData);
  JSONWriter wr{doc};
  T t;
  wr.writeTo(t);
  return t;
}

template <typename T>
QByteArray toJson(const T& t)
{
  JSONReader reader;
  reader.readFrom(t);
  return reader.toByteArray();
}
