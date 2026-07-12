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
  Scene,
};

enum class Flag : uint32_t
{
  // Grabs texture at the source instead of
  // asking it to render. Used for instance to get cubemap textures.
  GrabsFromSource = (1 << 0),
  SamplableDepth  = (1 << 1),

  // Sink expects a sampler2DArray (texture carries multiple layers).
  TextureArray    = (1 << 2),

  // Sink expects imageLoad/imageStore (storage image) rather than sampledTexture.
  StorageImage    = (1 << 3),

  // Buffer port carries indirect-draw arguments (QRhiDrawIndirectCommand[]).
  IndirectDraw    = (1 << 4),

  // Image port is a multiview texture array (one layer per view).
  MultiView       = (1 << 5),

  // Output port produces only depth (no color attachment).
  DepthOnly       = (1 << 6),

  // Buffer port is bound as a uniform buffer (UBO, std140) rather than as a
  // storage buffer (SSBO, std430). Used for `uniform_input` from upstream.
  UniformBuffer   = (1 << 7),

  // Sink expects a sampler3D (texture is a 3D volume).
  ThreeDimensional = (1 << 8),

  // Sink expects a samplerCube.
  Cubemap          = (1 << 9),
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
