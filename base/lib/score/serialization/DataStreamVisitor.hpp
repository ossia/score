#pragma once
#include <ossia/detail/small_vector.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <ossia/detail/flat_set.hpp>
#include <score/model/EntityBase.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/serialization/VisitorTags.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Optional.hpp>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <type_traits>
#include <vector>

namespace score
{
class ApplicationComponents;
}
#if defined(SCORE_DEBUG_DELIMITERS)
#  define SCORE_DEBUG_INSERT_DELIMITER \
    do                                 \
    {                                  \
      insertDelimiter();               \
    } while (0)
#  define SCORE_DEBUG_INSERT_DELIMITER2(Vis) \
    do                                       \
    {                                        \
      Vis.insertDelimiter();                 \
    } while (0)
#  define SCORE_DEBUG_CHECK_DELIMITER \
    do                                \
    {                                 \
      checkDelimiter();               \
    } while (0)
#  define SCORE_DEBUG_CHECK_DELIMITER2(Vis) \
    do                                      \
    {                                       \
      Vis.checkDelimiter();                 \
    } while (0)
#else
#  define SCORE_DEBUG_INSERT_DELIMITER
#  define SCORE_DEBUG_INSERT_DELIMITER2(Vis)
#  define SCORE_DEBUG_CHECK_DELIMITER
#  define SCORE_DEBUG_CHECK_DELIMITER2(Vis)
#endif

/**
 * \file DataStreamVisitor.hpp
 *
 * This file contains facilities
 * to serialize an object using QDataStream.
 *
 * Generally, it is used with QByteArrays, but it works with any QIODevice.
 */
template <typename T>
using enable_if_QDataStreamSerializable = typename std::enable_if_t<
    std::is_arithmetic<T>::value || std::is_same<T, QStringList>::value
    || std::is_same<T, QVector2D>::value || std::is_same<T, QVector3D>::value
    || std::is_same<T, QVector4D>::value || std::is_same<T, QPointF>::value
    || std::is_same<T, QPoint>::value || std::is_same<T, std::string>::value>;

template <class, class Enable = void>
struct is_QDataStreamSerializable : std::false_type
{
};

template <class T>
struct is_QDataStreamSerializable<
    T,
    enable_if_QDataStreamSerializable<typename std::decay<T>::type>>
    : std::true_type
{
};

SCORE_LIB_BASE_EXPORT QDataStream& operator<<(QDataStream& s, char c);
SCORE_LIB_BASE_EXPORT QDataStream& operator>>(QDataStream& s, char& c);

SCORE_LIB_BASE_EXPORT QDataStream&
operator<<(QDataStream& stream, const std::string& obj);
SCORE_LIB_BASE_EXPORT QDataStream&
operator>>(QDataStream& stream, std::string& obj);

class QIODevice;
class QStringList;

struct DataStreamInput
{
  QDataStream& stream;
};
#if (INTPTR_MAX == INT64_MAX) && !defined(__APPLE__) && !defined(_MSC_VER)
inline QDataStream& operator<<(QDataStream& s, uint64_t val)
{
  s << (quint64)val;
  return s;
}
inline QDataStream& operator>>(QDataStream& s, uint64_t& val)
{
  s >> (quint64&)val;
  return s;
}
inline QDataStream& operator<<(QDataStream& s, int64_t val)
{
  s << (qint64)val;
  return s;
}
inline QDataStream& operator>>(QDataStream& s, int64_t& val)
{
  s >> (qint64&)val;
  return s;
}
#endif
template <typename T>
DataStreamInput& operator<<(DataStreamInput& s, const T& obj)
{
  s.stream << obj;
  return s;
}

struct DataStreamOutput
{
  QDataStream& stream;
};

template <typename T>
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj)
{
  s.stream >> obj;
  return s;
}

class DataStreamReader;
class DataStreamWriter;
class DataStream
{
public:
  using Serializer = DataStreamReader;
  using Deserializer = DataStreamWriter;
  static constexpr SerializationIdentifier type()
  {
    return 2;
  }
};

class SCORE_LIB_BASE_EXPORT DataStreamReader : public AbstractVisitor
{
public:
  using type = DataStream;
  using is_visitor_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant()
  {
    return {*this, DataStream::type()};
  }

  DataStreamReader();
  DataStreamReader(QByteArray* array);
  DataStreamReader(QIODevice* dev);

  DataStreamReader(const DataStreamReader&) = delete;
  DataStreamReader& operator=(const DataStreamReader&) = delete;

  template <typename T>
  static auto marshall(const T& t)
  {
    QByteArray arr;
    DataStreamReader reader{&arr};
    reader.readFrom(t);
    return arr;
  }

  //! Called by code that wants to serialize.
  template <typename T>
  void readFrom(const T& obj)
  {
    readFrom_impl(obj, typename serialization_tag<T>::type{});
  }

  /**
   * @brief insertDelimiter
   *
   * Adds a delimiter that is to be checked by the reader.
   */
  void insertDelimiter()
  {
    m_stream << int32_t(0xDEADBEEF);
  }

  auto& stream()
  {
    return m_stream;
  }

  //! Serializable types should reimplement this method
  //! It is not to be called by user code.
  template <typename T>
  void read(const T&);

private:
  template <typename T>
  void readFrom_impl(const T& obj, visitor_template_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    TSerializer<DataStream, T>::readFrom(*this, obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_object_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    else
    {
      TSerializer<DataStream, IdentifiedObject<T>>::readFrom(*this, obj);
    }

    if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
        TSerializer<DataStream, T>::readFrom(*this, obj);
    else
        read(obj);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_entity_tag)
  {
    if constexpr(base_kind<T>::value)
    {
      readFrom_impl((const typename T::base_type&) obj, typename serialization_tag<T>::type{});
    }
    else
    {
      TSerializer<DataStream, score::Entity<T>>::readFrom(*this, obj);
    }

    if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
        TSerializer<DataStream, T>::readFrom(*this, obj);
    else
        read(obj);
  }

  template <typename T, typename Fun>
  void readFromAbstract(const T& obj, Fun f)
  {
    // We save in a byte array so that
    // we have a chance to save it as-is and reload it later
    // if the plug-in is not found on the system.
    QByteArray b;
    DataStream::Serializer sub{&b};

    // First read the key
    SCORE_DEBUG_INSERT_DELIMITER2(sub);
    sub.readFrom(obj.concreteKey().impl());
    SCORE_DEBUG_INSERT_DELIMITER2(sub);

    // Read our object
    f(sub);

    // Read the implementation of the derived class, through a virtual
    // function.
    obj.serialize_impl(sub.toVariant());

    // Finish our object
    SCORE_DEBUG_INSERT_DELIMITER2(sub);

    // Save the bundle
    m_stream << std::move(b);
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_tag)
  {
    readFromAbstract(obj, [&](DataStreamReader& sub) {
      // Read the implementation of the base object
      sub.read(obj);
    });
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_object_tag)
  {
    readFromAbstract(obj, [&](DataStreamReader& sub) {
      sub.readFrom_impl(obj, visitor_object_tag{});
    });
  }

  template <typename T>
  void readFrom_impl(const T& obj, visitor_abstract_entity_tag)
  {
    readFromAbstract(obj, [&](DataStreamReader& sub) {
      sub.readFrom_impl(obj, visitor_entity_tag{});
    });
  }

  //! Used to serialize general objects that won't fit in the other categories
  template <typename T>
  void readFrom_impl(const T& obj, visitor_default_tag)
  {
    read(obj);
  }

  //! Used to serialize enums.
  template <typename T>
  void readFrom_impl(const T& elt, visitor_enum_tag)
  {
    m_stream << (int32_t)elt;
  }

  QDataStream m_stream_impl;

public:
  const score::ApplicationComponents& components;
  DataStreamInput m_stream{m_stream_impl};
};

class SCORE_LIB_BASE_EXPORT DataStreamWriter : public AbstractVisitor
{
public:
  using type = DataStream;
  using is_visitor_tag = std::integral_constant<bool, true>;
  using is_deserializer_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant()
  {
    return {*this, DataStream::type()};
  }

  DataStreamWriter();
  DataStreamWriter(const DataStreamWriter&) = delete;
  DataStreamWriter& operator=(const DataStreamWriter&) = delete;

  DataStreamWriter(const QByteArray& array);
  DataStreamWriter(QIODevice* dev);

  template <typename T>
  static auto unmarshall(const QByteArray& arr)
  {
    T data;
    DataStreamWriter wrt{arr};
    wrt.writeTo(data);
    return data;
  }

  //! Serializable types should reimplement this method
  //! It is not to be called by user code.
  template <typename T>
  void write(T&);

  template <typename T>
  void writeTo(T& obj)
  {
    writeTo_impl(obj, typename serialization_tag<T>::type{});
  }

  /**
   * @brief checkDelimiter
   *
   * Checks if a delimiter is present at the current
   * stream position, and fails if it isn't.
   */
  void checkDelimiter()
  {
    int val{};
    m_stream >> val;

    if (val != int32_t(0xDEADBEEF))
    {
      SCORE_BREAKPOINT;
      throw std::runtime_error("Corrupt save file.");
    }
  }

  auto& stream()
  {
    return m_stream;
  }

private:
  template <typename T>
  void writeTo_impl(T& obj, visitor_template_tag)
  {
    TSerializer<DataStream, T>::writeTo(*this, obj);
  }

  template <typename T, typename OtherTag>
  void writeTo_impl(T& obj, OtherTag)
  {
    write(obj);
  }

  template <typename T>
  void writeTo_impl(T& elt, visitor_enum_tag)
  {
    int32_t e;
    m_stream >> e;
    elt = static_cast<T>(e);
  }

  QDataStream m_stream_impl;

public:
  const score::ApplicationComponents& components;
  DataStreamOutput m_stream{m_stream_impl};
};

// TODO instead why not add a score_serializable tag to our classes ?
template <
    typename T,
    std::enable_if_t<
        !std::is_arithmetic<T>::value
        && !std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator<<(QDataStream& stream, const T& obj)
{
  DataStreamReader reader{stream.device()};
  reader.readFrom(obj);
  return stream;
}

template <
    typename T,
    std::enable_if_t<
        !std::is_arithmetic<T>::value
        && !std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator>>(QDataStream& stream, T& obj)
{
  DataStreamWriter writer{stream.device()};
  writer.writeTo(obj);

  return stream;
}

template <typename U>
struct TSerializer<DataStream, Id<U>>
{
  static void readFrom(DataStream::Serializer& s, const Id<U>& obj)
  {
    s.stream() << obj.val();
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, Id<U>& obj)
  {
    int32_t val{};
    s.stream() >> val;
    obj.setVal(val);
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<DataStream, IdentifiedObject<T>>
{
  template <typename U>
  static void
  readFrom(DataStream::Serializer& s, const IdentifiedObject<U>& obj)
  {
    SCORE_DEBUG_INSERT_DELIMITER2(s);
    s.stream() << obj.objectName();
    SCORE_DEBUG_INSERT_DELIMITER2(s);
    s.readFrom(obj.id());
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  template <typename U>
  static void writeTo(DataStream::Deserializer& s, IdentifiedObject<U>& obj)
  {
    QString name;
    Id<T> id;
    SCORE_DEBUG_CHECK_DELIMITER2(s);
    s.stream() >> name;
    SCORE_DEBUG_CHECK_DELIMITER2(s);
    obj.setObjectName(name);
    s.writeTo(id);
    obj.setId(std::move(id));
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<DataStream, optional<T>>
{
  static void readFrom(DataStream::Serializer& s, const optional<T>& obj)
  {
    s.stream() << static_cast<bool>(obj);

    if (obj)
    {
      s.stream() << *obj;
    }
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, optional<T>& obj)
  {
    bool b{};
    s.stream() >> b;

    if (b)
    {
      T val;
      s.stream() >> val;
      obj = val;
    }
    else
    {
      obj = ossia::none;
    }
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<DataStream, QList<T>>
{
  static void readFrom(DataStream::Serializer& s, const QList<T>& obj)
  {
    s.stream() << obj;
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, QList<T>& obj)
  {
    s.stream() >> obj;
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, std::size_t N>
struct TSerializer<DataStream, std::array<T, N>>
{
  static void readFrom(DataStream::Serializer& s, const std::array<T, N>& arr)
  {
    for (std::size_t i = 0U; i < N; i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::array<T, N>& arr)
  {
    for (std::size_t i = 0U; i < N; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

#if defined(OSSIA_SMALL_VECTOR)
template <typename T, std::size_t N>
struct TSerializer<DataStream, ossia::small_vector<T, N>>
{
  static void
  readFrom(DataStream::Serializer& s, const ossia::small_vector<T, N>& arr)
  {
    s.stream() << (int32_t)arr.size();
    for (std::size_t i = 0U; i < arr.size(); i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void
  writeTo(DataStream::Deserializer& s, ossia::small_vector<T, N>& arr)
  {
    int32_t sz;

    s.stream() >> sz;
    arr.resize(sz);
    for (int32_t i = 0; i < sz; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, std::size_t N>
struct TSerializer<DataStream, ossia::static_vector<T, N>>
{
  static void
  readFrom(DataStream::Serializer& s, const ossia::static_vector<T, N>& arr)
  {
    s.stream() << (int32_t)arr.size();
    for (int32_t i = 0U; i < arr.size(); i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void
  writeTo(DataStream::Deserializer& s, ossia::static_vector<T, N>& arr)
  {
    int32_t sz;

    s.stream() >> sz;
    arr.resize(sz);
    for (int32_t i = 0U; i < sz; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};
#endif

template <typename T, typename Alloc>
struct TSerializer<
    DataStream,
    std::vector<T, Alloc>,
    std::enable_if_t<!is_QDataStreamSerializable<
        typename std::vector<T, Alloc>::value_type>::value>>
{
  static void
  readFrom(DataStream::Serializer& s, const std::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for (const auto& elt : vec)
      s.readFrom(elt);

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<T, Alloc>& vec)
  {
    int32_t n;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for (int32_t i = 0; i < n; i++)
    {
      s.writeTo(vec[i]);
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, typename Alloc>
struct TSerializer<
    DataStream,
    std::vector<T, Alloc>,
    std::enable_if_t<is_QDataStreamSerializable<
        typename std::vector<T, Alloc>::value_type>::value>>
{
  static void
  readFrom(DataStream::Serializer& s, const std::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for (const auto& elt : vec)
      s.stream() << elt;

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<T, Alloc>& vec)
  {
    int32_t n = 0;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for (int32_t i = 0; i < n; i++)
    {
      s.stream() >> vec[i];
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename U>
struct TSerializer<DataStream, UuidKey<U>>
{
  static void readFrom(DataStream::Serializer& s, const UuidKey<U>& uid)
  {
    s.stream().stream.writeRawData(
        (const char*)uid.impl().data.data(), sizeof(uid.impl().data));
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, UuidKey<U>& uid)
  {
    s.stream().stream.readRawData(
        (char*)uid.impl().data.data(), sizeof(uid.impl().data));
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, typename U>
struct TSerializer<DataStream, score::hash_map<T, U>>
{
  static void
  readFrom(DataStream::Serializer& s, const score::hash_map<T, U>& obj)
  {
    auto& st = s.stream();
    st << (int32_t)obj.size();
    for (const auto& e : obj)
    {
      st << e.first << e.second;
    }
  }

  static void writeTo(DataStream::Deserializer& s, score::hash_map<T, U>& obj)
  {
    auto& st = s.stream();
    int32_t n;
    st >> n;
    for (int32_t i = 0; i < n; i++)
    {
      T key;
      U value;
      st >> key >> value;
      obj.emplace(std::move(key), std::move(value));
    }
  }
};

template <typename T>
struct TSerializer<DataStream, ossia::flat_set<T>>
{
  using type = ossia::flat_set<T>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.stream() << (int32_t)obj.size();
    for (const auto& e : obj)
      s.stream() << e;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    int32_t n;
    s.stream() >> n;
    for (; n-- > 0;)
    {
      T val;
      s.stream() >> val;
      obj.insert(std::move(val));
    }
  }
};

template <typename T, typename U>
struct TSerializer<DataStream, std::pair<T, U>>
{
  using type = std::pair<T, U>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.stream() << obj.first << obj.second;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    s.stream() >> obj.first >> obj.second;
  }
};

Q_DECLARE_METATYPE(DataStreamReader*)
Q_DECLARE_METATYPE(DataStreamWriter*)
W_REGISTER_ARGTYPE(DataStreamReader*)
W_REGISTER_ARGTYPE(DataStreamWriter*)
