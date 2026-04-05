#pragma once
namespace score::gfx
{
struct image
{
};
struct geometry
{
};
enum class Types : int8_t
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
  Geometry,
  Buffer,
};

enum class Flag : uint32_t
{
  // Grabs texture at the source instead of
  // asking it to render. Used for instance to get cubemap textures.
  GrabsFromSource = (1 << 0),
  SamplableDepth = (1 << 1)
};

static constexpr inline Flag operator&(Flag lhs, Flag rhs)
{
  return (Flag)(((uint32_t)lhs) & ((uint32_t)rhs));
}
static constexpr inline Flag operator|(Flag lhs, Flag rhs)
{
  return (Flag)(((uint32_t)lhs) | ((uint32_t)rhs));
}
static constexpr inline Flag operator^(Flag lhs, Flag rhs)
{
  return (Flag)(((uint32_t)lhs) ^ ((uint32_t)rhs));
}
}
