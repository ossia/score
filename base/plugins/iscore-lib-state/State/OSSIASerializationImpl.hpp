#pragma once

#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>
#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/serialization/VariantSerialization.hpp>
#include <brigand/algorithms/for_each.hpp>
#include <State/Value.hpp>
#include <iscore_lib_state_export.h>
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
JSON_METADATA(ossia::value, "Generic")
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

ISCORE_DECL_VALUE_TYPE(int)
ISCORE_DECL_VALUE_TYPE(float)
ISCORE_DECL_VALUE_TYPE(bool)
ISCORE_DECL_VALUE_TYPE(char)
ISCORE_DECL_VALUE_TYPE(ossia::impulse)
ISCORE_DECL_VALUE_TYPE(std::string)
ISCORE_DECL_VALUE_TYPE(ossia::vec2f)
ISCORE_DECL_VALUE_TYPE(ossia::vec3f)
ISCORE_DECL_VALUE_TYPE(ossia::vec4f)

template<>
struct is_custom_serialized<ossia::vector_domain> : public std::true_type { };
template<std::size_t N>
struct is_custom_serialized<ossia::vecf_domain<N>> : public std::true_type { };
template<typename T, std::size_t N>
struct is_custom_serialized<std::array<T, N>> : public std::true_type { };

template<>
struct is_custom_serialized<ossia::value_variant_type> : public std::true_type {};

template<>
struct is_custom_serialized<ossia::domain_base_variant> : public std::true_type {};

template<typename T>
struct typeholder { using type = T; };
