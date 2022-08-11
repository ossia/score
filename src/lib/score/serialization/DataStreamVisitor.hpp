#pragma once
#include <score/model/Identifier.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/CommonTypes.hpp>
#include <score/serialization/DataStreamFwd.hpp>
#include <score/serialization/DataStreamHelpers.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/serialization/VisitorTags.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/small_vector.hpp>

#include <verdigris>

template <typename model>
class IdentifiedObject;
namespace score
{
template <typename model>
class Entity;
class ApplicationComponents;
}

class SCORE_LIB_BASE_EXPORT DataStreamReader : public AbstractVisitor
{
public:
  using type = DataStream;
  using is_visitor_tag = std::integral_constant<bool, true>;

  VisitorVariant toVariant() { return {*this, DataStream::type()}; }

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
  void readFrom(const score::Entity<T>& obj)
  {
    TSerializer<DataStream, score::Entity<T>>::readFrom(*this, obj);
  }

  template <typename T>
  void readFrom(const IdentifiedObject<T>& obj)
  {
    TSerializer<DataStream, IdentifiedObject<T>>::readFrom(*this, obj);
  }

  void readFrom(const QByteArray& obj) { m_stream_impl << obj; }

  template <typename T>
  void readFrom(const T& obj)
  {
    static constexpr bool has_base = base_kind<T>::value;

    if constexpr(has_base)
    {
      readFrom((const typename T::base_type&)obj);
    }
    else
    {
      if constexpr(
          (is_template<T>::value && !is_abstract_base<T>::value
           && !is_identified_object<T>::value)
          || is_custom_serialized<T>::value)
      {
        TSerializer<DataStream, T>::readFrom(*this, obj);
      }
      else if constexpr(
          is_identified_object<T>::value && !is_entity<T>::value
          && !is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        TSerializer<DataStream, typename T::object_type>::readFrom(*this, obj);

        if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
          TSerializer<DataStream, T>::readFrom(*this, obj);
        else
          read(obj);
      }
      else if constexpr(
          is_entity<T>::value && !is_abstract_base<T>::value
          && !is_custom_serialized<T>::value)
      {
        TSerializer<DataStream, typename T::entity_type>::readFrom(*this, obj);

        if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
          TSerializer<DataStream, T>::readFrom(*this, obj);
        else
          read(obj);
      }
      else if constexpr(
          !is_identified_object<T>::value && is_abstract_base<T>::value
          && !is_custom_serialized<T>::value)
      {
        readFromAbstract(obj, [](DataStreamReader& sub, const T& obj) {
          // Read the implementation of the base object
          sub.read(obj);
        });
      }
      else if constexpr(
          is_identified_object<T>::value && !is_entity<T>::value
          && is_abstract_base<T>::value && !is_custom_serialized<T>::value)
      {
        readFromAbstract(obj, [](DataStreamReader& sub, const T& obj) {
          TSerializer<DataStream, IdentifiedObject<T>>::readFrom(sub, obj);

          if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
            TSerializer<DataStream, T>::readFrom(sub, obj);
          else
            sub.read(obj);
        });
      }
      else if constexpr(
          is_entity<T>::value && is_abstract_base<T>::value
          && !is_custom_serialized<T>::value)
      {
        readFromAbstract(obj, [](DataStreamReader& sub, const T& obj) {
          TSerializer<DataStream, score::Entity<T>>::readFrom(sub, obj);

          if constexpr(is_custom_serialized<T>::value || is_template<T>::value)
            TSerializer<DataStream, T>::readFrom(sub, obj);
          else
            sub.read(obj);
        });
      }
      else if constexpr(std::is_enum<T>::value)
      {
        static_assert(check_enum_size<T>::value);
        m_stream << (int32_t)obj;
      }
      else
      {
        //! Used to serialize general objects that won't fit in the other
        //! categories
        read(obj);
      }
    }
  }

  /**
   * @brief insertDelimiter
   *
   * Adds a delimiter that is to be checked by the reader.
   */
  void insertDelimiter() { m_stream << int32_t(0xDEADBEEF); }

  auto& stream() { return m_stream; }

  //! Serializable types should reimplement this method
  //! It is not to be called by user code.
  template <typename T>
  void read(const T&);

private:
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
    f(sub, obj);

    // Read the implementation of the derived class, through a virtual
    // function.
    obj.serialize_impl(sub.toVariant());

    // Finish our object
    SCORE_DEBUG_INSERT_DELIMITER2(sub);

    // Save the bundle
    m_stream << std::move(b);
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

  VisitorVariant toVariant() { return {*this, DataStream::type()}; }

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

  void writeTo(QByteArray& obj) { m_stream_impl >> obj; }

  template <typename T>
  void writeTo(T& obj)
  {
    if constexpr(
        (is_template<T>::value && !is_abstract_base<T>::value
         && !is_identified_object<T>::value)
        || is_custom_serialized<T>::value)
    {
      TSerializer<DataStream, T>::writeTo(*this, obj);
    }
    else if constexpr(std::is_enum<T>::value)
    {
      static_assert(check_enum_size<T>::value);
      int32_t e;
      m_stream >> e;
      obj = static_cast<T>(e);
    }
    else
    {
      write(obj);
    }
  }

  /**
   * @brief checkDelimiter
   *
   * Checks if a delimiter is present at the current
   * stream position, and fails if it isn't.
   */
  void checkDelimiter();

  auto& stream() { return m_stream; }

  const score::ApplicationComponents& components;
  DataStreamOutput m_stream{m_stream_impl};

private:
  QDataStream m_stream_impl;
};

template <typename T, std::enable_if_t<!std::is_enum_v<T>>*>
DataStreamInput& operator<<(DataStreamInput& s, const T& obj)
{
  DataStreamReader reader{s.stream.device()};
  reader.readFrom(obj);
  return s;
}

template <typename T, std::enable_if_t<!std::is_enum_v<T>>*>
DataStreamOutput& operator>>(DataStreamOutput& s, T& obj)
{
  DataStreamWriter writer{s.stream.device()};
  writer.writeTo(obj);
  return s;
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
  static void readFrom(DataStream::Serializer& s, const IdentifiedObject<U>& obj)
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
struct TSerializer<DataStream, std::optional<T>>
{
  static void readFrom(DataStream::Serializer& s, const std::optional<T>& obj)
  {
    s.stream() << static_cast<bool>(obj);

    if(obj)
    {
      s.stream() << *obj;
    }
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::optional<T>& obj)
  {
    bool b{};
    s.stream() >> b;

    if(b)
    {
      T val;
      s.stream() >> val;
      obj = val;
    }
    else
    {
      obj = std::nullopt;
    }
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<DataStream, QList<T>>
{
  static void readFrom(DataStream::Serializer& s, const QList<T>& obj) = delete;
  static void writeTo(DataStream::Deserializer& s, QList<T>& obj) = delete;
};

template <typename T, std::size_t N>
struct TSerializer<DataStream, std::array<T, N>>
{
  static void readFrom(DataStream::Serializer& s, const std::array<T, N>& arr)
  {
    for(std::size_t i = 0U; i < N; i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::array<T, N>& arr)
  {
    for(std::size_t i = 0U; i < N; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

#if defined(OSSIA_SMALL_VECTOR)
template <typename T, std::size_t N>
struct TSerializer<DataStream, ossia::small_vector<T, N>>
{
  static void readFrom(DataStream::Serializer& s, const ossia::small_vector<T, N>& arr)
  {
    s.stream() << (int32_t)arr.size();
    for(std::size_t i = 0U; i < arr.size(); i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, ossia::small_vector<T, N>& arr)
  {
    int32_t sz;

    s.stream() >> sz;
    arr.resize(sz);
    for(int32_t i = 0; i < sz; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, std::size_t N>
struct TSerializer<DataStream, ossia::static_vector<T, N>>
{
  static void readFrom(DataStream::Serializer& s, const ossia::static_vector<T, N>& arr)
  {
    s.stream() << (int32_t)arr.size();
    for(int32_t i = 0U; i < arr.size(); i++)
      s.stream() << arr[i];

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, ossia::static_vector<T, N>& arr)
  {
    int32_t sz;

    s.stream() >> sz;
    arr.resize(sz);
    for(int32_t i = 0U; i < sz; i++)
      s.stream() >> arr[i];

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, std::size_t N>
OSSIA_INLINE QDataStream&
operator<<(QDataStream& stream, const ossia::small_vector<T, N>& obj)
{
  DataStreamReader reader{stream.device()};
  reader.readFrom(obj);
  return stream;
}

template <typename T, std::size_t N>
OSSIA_INLINE QDataStream& operator>>(QDataStream& stream, ossia::small_vector<T, N>& obj)
{
  DataStreamWriter writer{stream.device()};
  writer.writeTo(obj);
  return stream;
}

#endif

template <typename T, typename Alloc>
struct TSerializer<
    DataStream, std::vector<T, Alloc>,
    std::enable_if_t<
        !is_QDataStreamSerializable<typename std::vector<T, Alloc>::value_type>::value
        && !std::is_pointer_v<T> && !is_shared_ptr_v<T>>>
{
  static void readFrom(DataStream::Serializer& s, const std::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for(const auto& elt : vec)
      s.readFrom(elt);

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<T, Alloc>& vec)
  {
    int32_t n;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for(int32_t i = 0; i < n; i++)
    {
      s.writeTo(vec[i]);
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, typename Alloc>
struct TSerializer<
    DataStream, boost::container::vector<T, Alloc>,
    std::enable_if_t<
        !is_QDataStreamSerializable<typename std::vector<T, Alloc>::value_type>::value
        && !std::is_pointer_v<T> && !is_shared_ptr_v<T>>>
{
  static void
  readFrom(DataStream::Serializer& s, const boost::container::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for(const auto& elt : vec)
      s.readFrom(elt);

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void
  writeTo(DataStream::Deserializer& s, boost::container::vector<T, Alloc>& vec)
  {
    int32_t n;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for(int32_t i = 0; i < n; i++)
    {
      s.writeTo(vec[i]);
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <>
struct TSerializer<DataStream, std::vector<bool>>
{
  static void readFrom(DataStream::Serializer& s, const std::vector<bool>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for(bool elt : vec)
      s.stream() << elt;

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<bool>& vec)
  {
    int32_t n;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for(int i = 0; i < n; i++)
    {
      bool b;
      s.stream() >> b;
      vec[i] = b;
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, typename Alloc>
struct TSerializer<
    DataStream, std::vector<T, Alloc>,
    std::enable_if_t<
        is_QDataStreamSerializable<typename std::vector<T, Alloc>::value_type>::value>>
{
  static void readFrom(DataStream::Serializer& s, const std::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for(const auto& elt : vec)
      s.stream() << elt;

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<T, Alloc>& vec)
  {
    int32_t n = 0;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for(int32_t i = 0; i < n; i++)
    {
      s.stream() >> vec[i];
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T, typename Alloc>
struct TSerializer<
    DataStream, boost::container::vector<T, Alloc>,
    std::enable_if_t<
        is_QDataStreamSerializable<typename std::vector<T, Alloc>::value_type>::value>>
{
  static void
  readFrom(DataStream::Serializer& s, const boost::container::vector<T, Alloc>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for(const auto& elt : vec)
      s.stream() << elt;

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void
  writeTo(DataStream::Deserializer& s, boost::container::vector<T, Alloc>& vec)
  {
    int32_t n = 0;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for(int32_t i = 0; i < n; i++)
    {
      s.stream() >> vec[i];
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<DataStream, ossia::flat_set<T>>
{
  using type = ossia::flat_set<T>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.stream() << (int32_t)obj.size();
    for(const auto& e : obj)
      s.stream() << e;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    int32_t n;
    s.stream() >> n;
    for(; n-- > 0;)
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

template <typename T>
struct TSerializer<DataStream, std::shared_ptr<T>>
{
  using type = std::shared_ptr<T>;
  static void readFrom(DataStream::Serializer& s, const type& obj) { SCORE_ABORT; }

  static void writeTo(DataStream::Deserializer& s, type& obj) { SCORE_ABORT; }
};

Q_DECLARE_METATYPE(DataStreamReader*)
Q_DECLARE_METATYPE(DataStreamWriter*)
W_REGISTER_ARGTYPE(DataStreamReader*)
W_REGISTER_ARGTYPE(DataStreamWriter*)
