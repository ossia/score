#pragma once
#include <ossia/network/value/value.hpp>
#include <score/tools/Debug.hpp>
#include <QColor>

#include <array>
#include <variant>
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

using ValueVariant
    = std::variant<std::monostate, float, ossia::vec2f, ossia::vec3f, ossia::vec4f, image>;
