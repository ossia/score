#pragma once

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/for_each.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>


#include <score_lib_state_export.h>
JSON_METADATA(ossia::impulse, "Impulse")
JSON_METADATA(int32_t, "Int")
JSON_METADATA(char, "Char")
JSON_METADATA(bool, "Bool")
JSON_METADATA(float, "Float")
JSON_METADATA(ossia::vec2f, "Vec2f")
JSON_METADATA(ossia::vec3f, "Vec3f")
JSON_METADATA(ossia::vec4f, "Vec4f")
JSON_METADATA(std::vector<ossia::value>, "Tuple")
JSON_METADATA(std::string, "String")
JSON_METADATA(ossia::domain_base<ossia::impulse>, "Impulse")
JSON_METADATA(ossia::domain_base<int32_t>, "Int")
JSON_METADATA(ossia::domain_base<char>, "Char")
JSON_METADATA(ossia::domain_base<bool>, "Bool")
JSON_METADATA(ossia::domain_base<float>, "Float")
JSON_METADATA(ossia::vecf_domain<2>, "Vec2f")
JSON_METADATA(ossia::vecf_domain<3>, "Vec3f")
JSON_METADATA(ossia::vecf_domain<4>, "Vec4f")
JSON_METADATA(ossia::vector_domain, "Tuple")
JSON_METADATA(ossia::domain_base<std::string>, "String")
JSON_METADATA(ossia::domain_base<ossia::value>, "Generic")

template <>
struct is_custom_serialized<ossia::vector_domain> : public std::true_type
{
};
template <std::size_t N>
struct is_custom_serialized<ossia::vecf_domain<N>> : public std::true_type
{
};
template <typename T, std::size_t N>
struct is_custom_serialized<std::array<T, N>> : public std::true_type
{
};

template <>
struct is_custom_serialized<ossia::value_variant_type> : public std::true_type
{
};

template <>
struct is_custom_serialized<ossia::domain_base_variant> : public std::true_type
{
};

template <typename T>
struct typeholder
{
  using type = T;
};
