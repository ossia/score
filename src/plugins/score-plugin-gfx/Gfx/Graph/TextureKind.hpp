#pragma once
#include <QSize>
#include <QString>

#include <score_plugin_gfx_export.h>

#include <cstdint>

class QRhi;
class QRhiTexture;

namespace score::gfx
{
/**
 * What a texture holds: one image, or several of the same size. A texture
 * port does not say it; the texture a producer hands out does, through its
 * flags.
 */
enum class TextureKind : uint8_t
{
  Texture2D,
  Texture3D,
  Array,
  Cube
};
inline constexpr int texture_kind_count = 4;

//! A texture's kind, its size, and how many images it holds: the slices of a
//! 3D texture, the layers of an array, the six faces of a cube, one for 2D.
struct SCORE_PLUGIN_GFX_EXPORT TextureShape
{
  TextureKind kind{TextureKind::Texture2D};
  int width{};
  int height{};
  int layers{1};

  static TextureShape of(const QRhiTexture& tex) noexcept;

  bool empty() const noexcept { return width <= 0 || height <= 0; }

  //! In one word, for a monitor that hands it to the views.
  uint64_t pack() const noexcept;
  static TextureShape unpack(uint64_t v) noexcept;

  //! "3D 64x64x32", "array 8 layers 256x256", "cube 512", "2D 640x480".
  QString description() const;
  //! "slice 12", "layer 3", "face +X"; empty for a 2D texture.
  QString layerName(int layer) const;

  bool operator==(const TextureShape&) const noexcept = default;
};

/**
 * How the views show a texture that holds several images: one of them, or
 * as many as fit side by side.
 */
enum class TextureLayout : uint8_t
{
  Single,
  Overview
};

/**
 * The 2D image the views show of a texture.
 *
 * Single shows one slice, layer or face at the texture's size. Overview tiles
 * up to max_tiles slices or layers, evenly spaced, row by row from the top
 * left, and unfolds a cube as a cross:
 *
 *        +Y
 *    -X  +Z  +X  -Z
 *        -Y
 *
 * The shaders of shownTextureGlsl() lay it out the same way.
 */
struct SCORE_PLUGIN_GFX_EXPORT ShownTexture
{
  static constexpr int max_tiles = 16;

  TextureShape shape;
  TextureLayout layout{TextureLayout::Single};
  //! The slice, layer or face Single shows
  int index{};

  //! For a 3D texture, `slice` from 0 to 1 across its depth; for an array,
  //! `layer`; for a cube, `layer` is the face, 0 to 5 for +X -X +Y -Y +Z -Z.
  static ShownTexture
  of(TextureShape shape, TextureLayout layout, double slice, int layer) noexcept;

  //! Whether there is more than one image to lay out.
  bool layered() const noexcept { return shape.kind != TextureKind::Texture2D; }
  bool overview() const noexcept
  {
    return layered() && layout == TextureLayout::Overview;
  }

  //! Images tiled by Overview, and in how many columns and rows.
  int tiles() const noexcept;
  int columns() const noexcept;
  int rows() const noexcept;
  //! The slice, layer or face of the i-th tile of Overview.
  int tileLayer(int i) const noexcept;

  //! Size of the image shown, at most `max_side` pixels wide and high.
  QSize size(int max_side) const noexcept;

  //! What a point of the shown image is, in [0, 1] from its top left: the
  //! slice, layer or face, and where in it, in [0, 1] from its top left.
  struct Point
  {
    int layer{-1};
    double u{}, v{};
  };
  //! layer -1: the point falls in no tile.
  Point locate(double u, double v) const noexcept;
};

/**
 * GLSL that reads a texture of `kind` as the image ShownTexture describes.
 *
 * It declares `layout(binding = binding) uniform <sampler> name` and:
 *
 *     // `uv` and `p` address the shown image in memory, as a render target
 *     // or a storage image holds it. `flip`: QRhi::isYUpInNDC(), where a 2D
 *     // image is held bottom row first (see RowOrder.hpp): the tiles still
 *     // go from the top left of what the image shows.
 *     vec4 shownSample(vec2 uv, int layout, int index, bool flip, out bool inside);
 *     vec4 shownTexel(ivec2 p, ivec2 size, int layout, int index, bool flip, out bool inside);
 *
 * shownSample filters, shownTexel reads the nearest texel. `inside` is false
 * between the tiles. For a 2D texture they are a textureLod() at `uv` and a
 * texelFetch() at `p`.
 */
SCORE_PLUGIN_GFX_EXPORT
QString shownTextureGlsl(TextureKind kind, int binding, const char* name = "inTex");
}
