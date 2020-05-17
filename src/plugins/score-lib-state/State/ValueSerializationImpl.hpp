#pragma once
#include <State/OSSIASerializationImpl.hpp>

#include <ossia/detail/size.hpp>

#include <brigand/sequences/list.hpp>

template <typename... Args>
struct tl
{
};
template <>
struct TSerializer<JSONObject, ossia::value_variant_type>
{
  using var_t = ossia::value_variant_type;

  using value_type_list = tl<
      float,
      int,
      ossia::vec2f,
      ossia::vec3f,
      ossia::vec4f,
      ossia::impulse,
      bool,
      std::string,
      std::vector<ossia::value>,
      char>;

  static void readFrom(JSONObject::Serializer& s, const var_t& var)
  {
    s.stream.StartObject();
    if ((quint64)var.which() != (quint64)var.npos)
    {
      ossia::for_each_type(value_type_list{}, VariantJSONSerializer<var_t>{s, var});
    }
    s.stream.EndObject();
  }

  static void writeTo(JSONObject::Deserializer& s, var_t& var)
  {
    if (s.base.MemberCount() == 0)
      return;
    ossia::for_each_type(value_type_list{}, VariantJSONDeserializer<var_t>{s, var});
  }
};

//////////// Value Variant serialization /////////////
template <typename Functor>
void apply_typeonly(
    Functor&& functor,
    ossia::value_variant_type::Type type,
    ossia::value_variant_type& var)
{
  using namespace ossia;
  switch (type)
  {
    case value_variant_type::Type::Type0:
      return functor(typeholder<float>{}, var);
    case value_variant_type::Type::Type1:
      return functor(typeholder<int>{}, var);
    case value_variant_type::Type::Type2:
      return functor(typeholder<ossia::vec2f>{}, var);
    case value_variant_type::Type::Type3:
      return functor(typeholder<ossia::vec3f>{}, var);
    case value_variant_type::Type::Type4:
      return functor(typeholder<ossia::vec4f>{}, var);
    case value_variant_type::Type::Type5:
      return functor(typeholder<ossia::impulse>{}, var);
    case value_variant_type::Type::Type6:
      return functor(typeholder<bool>{}, var);
    case value_variant_type::Type::Type7:
      return functor(typeholder<std::string>{}, var);
    case value_variant_type::Type::Type8:
      return functor(typeholder<std::vector<ossia::value>>{}, var);
    case value_variant_type::Type::Type9:
      return functor(typeholder<char>{}, var);
    default:
      throw;
  }
}

struct ValueVariantDatastreamSerializer
{
  DataStream::Serializer& s;
  template <typename T>
  void operator()(const T& value)
  {
    s.stream() << value;
  }
};

template <>
struct TSerializer<DataStream, ossia::value_variant_type>
{
  using var_t = ossia::value_variant_type;
  static void readFrom(DataStream::Serializer& s, const var_t& var)
  {
    s.stream() << (quint64)var.which();

    if (var)
    {
      ossia::apply_nonnull(ValueVariantDatastreamSerializer{s}, var);
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, var_t& var)
  {
    quint64 which;
    s.stream() >> which;

    if (which != (quint64)var.npos)
    {
      apply_typeonly(
          [&](auto type, var_t& var) {
            typename decltype(type)::type value;
            s.stream() >> value;
            var = std::move(value);
          },
          (var_t::Type)which,
          var);
    }
    s.checkDelimiter();
  }
};

/////////

template <>
void DataStreamReader::read(const ossia::impulse& value)
{
}

template <>
void DataStreamWriter::write(ossia::impulse& value)
{
}

template <>
void JSONReader::read(const ossia::impulse& value)
{
  stream.Null();
}

template <>
void JSONWriter::write(ossia::impulse& value)
{
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::value& n)
{
  writeTo((ossia::value_variant_type&)n.v);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const ossia::value& n)
{
  TSerializer<JSONObject, ossia::value_variant_type>::readFrom(*this, n.v);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(ossia::value& n)
{
  TSerializer<JSONObject, ossia::value_variant_type>::writeTo(*this, n.v);
}
