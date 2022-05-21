#pragma once
#include <score/tools/Debug.hpp>

#include <ossia/network/value/vec.hpp>
#include <ossia/detail/variant.hpp>

#include <QColor>

#include <array>

namespace score::gfx
{
struct image
{
};
enum class Types
{
  Empty,
  Int,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Image,
  Audio,
  Camera,
};

using ValueVariant = ossia::variant<
    ossia::monostate,
    float,
    ossia::vec2f,
    ossia::vec3f,
    ossia::vec4f,
    image>;
}
