#include "TextureKind.hpp"

#include <QtGui/private/qrhi_p.h>

#include <cmath>

#include <algorithm>

namespace score::gfx
{
TextureShape TextureShape::of(const QRhiTexture& tex) noexcept
{
  TextureShape s;
  const QSize sz = tex.pixelSize();
  s.width = sz.width();
  s.height = sz.height();
  const auto flags = tex.flags();
  if(flags.testFlag(QRhiTexture::CubeMap))
  {
    s.kind = TextureKind::Cube;
    s.layers = 6;
  }
  else if(flags.testFlag(QRhiTexture::ThreeDimensional))
  {
    s.kind = TextureKind::Texture3D;
    s.layers = std::max(1, tex.depth());
  }
  else if(flags.testFlag(QRhiTexture::TextureArray))
  {
    s.kind = TextureKind::Array;
    s.layers = std::max(1, tex.arraySize());
  }
  return s;
}

// kind: 4 bits, width and height: 20 bits each, layers: 20 bits
uint64_t TextureShape::pack() const noexcept
{
  constexpr uint64_t m = (1u << 20) - 1;
  return uint64_t(kind) | (uint64_t(std::clamp(width, 0, int(m))) << 4)
         | (uint64_t(std::clamp(height, 0, int(m))) << 24)
         | (uint64_t(std::clamp(layers, 0, int(m))) << 44);
}

TextureShape TextureShape::unpack(uint64_t v) noexcept
{
  constexpr uint64_t m = (1u << 20) - 1;
  TextureShape s;
  s.kind = TextureKind(std::min<uint64_t>(v & 0xF, texture_kind_count - 1));
  s.width = int((v >> 4) & m);
  s.height = int((v >> 24) & m);
  s.layers = int((v >> 44) & m);
  return s;
}

QString TextureShape::description() const
{
  switch(kind)
  {
    case TextureKind::Texture3D:
      return QStringLiteral("3D %1x%2x%3").arg(width).arg(height).arg(layers);
    case TextureKind::Array:
      return QStringLiteral("array %1 layers %2x%3").arg(layers).arg(width).arg(height);
    case TextureKind::Cube:
      return QStringLiteral("cube %1").arg(width);
    default:
      return QStringLiteral("2D %1x%2").arg(width).arg(height);
  }
}

QString TextureShape::layerName(int layer) const
{
  switch(kind)
  {
    case TextureKind::Texture3D:
      return QStringLiteral("slice %1").arg(layer);
    case TextureKind::Array:
      return QStringLiteral("layer %1").arg(layer);
    case TextureKind::Cube: {
      static constexpr const char* faces[6]{"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
      return QStringLiteral("face %1").arg(
          QLatin1String(faces[std::clamp(layer, 0, 5)]));
    }
    default:
      return {};
  }
}

ShownTexture ShownTexture::of(
    TextureShape shape, TextureLayout layout, double slice, int layer) noexcept
{
  ShownTexture s{.shape = shape, .layout = layout};
  const int n = std::max(1, shape.layers);
  if(shape.kind == TextureKind::Texture3D)
    s.index
        = std::clamp(int(std::lround(std::clamp(slice, 0., 1.) * (n - 1))), 0, n - 1);
  else if(shape.kind != TextureKind::Texture2D)
    s.index = std::clamp(layer, 0, n - 1);
  return s;
}

int ShownTexture::tiles() const noexcept
{
  if(shape.kind == TextureKind::Cube)
    return 6;
  return std::clamp(shape.layers, 1, max_tiles);
}

int ShownTexture::columns() const noexcept
{
  if(shape.kind == TextureKind::Cube)
    return 4;
  const int n = tiles();
  int c = 1;
  while(c * c < n)
    c++;
  return c;
}

int ShownTexture::rows() const noexcept
{
  if(shape.kind == TextureKind::Cube)
    return 3;
  const int c = columns();
  return (tiles() + c - 1) / c;
}

int ShownTexture::tileLayer(int i) const noexcept
{
  const int n = tiles();
  const int layers = std::max(1, shape.layers);
  if(n <= 1)
    return 0;
  return (i * (layers - 1) + (n - 1) / 2) / (n - 1);
}

QSize ShownTexture::size(int max_side) const noexcept
{
  const int w = std::max(1, shape.width), h = std::max(1, shape.height);
  QSize sz{w, h};
  if(overview())
    sz = QSize{w * columns(), h * rows()};
  if(sz.width() > max_side || sz.height() > max_side)
    sz = sz.scaled(max_side, max_side, Qt::KeepAspectRatio);
  return sz.expandedTo({1, 1});
}

namespace
{
//! The face of a cell of the cross, -1 for the cells around it.
int crossFace(int col, int row) noexcept
{
  if(row == 1)
  {
    static constexpr int middle[4]{1, 4, 0, 5};
    return middle[std::clamp(col, 0, 3)];
  }
  if(col == 1)
    return row == 0 ? 2 : 3;
  return -1;
}
}

ShownTexture::Point ShownTexture::locate(double u, double v) const noexcept
{
  if(!overview())
    return {layered() ? index : 0, u, v};
  const int cols = columns(), rws = rows();
  const double gx = std::clamp(u, 0., 1.) * cols, gy = std::clamp(v, 0., 1.) * rws;
  const int cx = std::clamp(int(gx), 0, cols - 1), cy = std::clamp(int(gy), 0, rws - 1);
  Point p{-1, gx - cx, gy - cy};
  if(shape.kind == TextureKind::Cube)
    p.layer = crossFace(cx, cy);
  else if(const int i = cy * cols + cx; i < tiles())
    p.layer = tileLayer(i);
  return p;
}

QString shownTextureGlsl(TextureKind kind, int binding, const char* name)
{
  const QString n = QString::fromLatin1(name);
  if(kind == TextureKind::Texture2D)
  {
    return QStringLiteral(R"_(
layout(binding = %1) uniform sampler2D %2;
vec4 shownSample(vec2 uv, int layout_, int index, bool flip, out bool inside)
{
  inside = true;
  return textureLod(%2, uv, 0.0);
}
vec4 shownTexel(ivec2 p, ivec2 size, int layout_, int index, bool flip, out bool inside)
{
  inside = true;
  return texelFetch(%2, p, 0);
}
)_")
        .arg(binding)
        .arg(n);
  }

  const char* sampler = kind == TextureKind::Texture3D ? "sampler3D"
                        : kind == TextureKind::Array   ? "sampler2DArray"
                                                       : "samplerCube";
  QString s = QStringLiteral("\nlayout(binding = %1) uniform %2 %3;\n")
                  .arg(binding)
                  .arg(QLatin1String(sampler))
                  .arg(n);
  s += QStringLiteral("const int shown_max_tiles = %1;\n").arg(ShownTexture::max_tiles);

  if(kind == TextureKind::Cube)
  {
    s += QStringLiteral(R"_(
int shown_layers() { return 6; }
ivec2 shown_tile_size() { return textureSize(%1, 0); }
void shown_grid(out int tiles, out int cols, out int rows) { tiles = 6; cols = 4; rows = 3; }
// The cross: +Y above +Z, -X +Z +X -Z across, -Y below
int shown_tile(ivec2 cell, int tiles, int cols)
{
  if(cell.y == 1)
    return cell.x == 0 ? 1 : cell.x == 1 ? 4 : cell.x == 2 ? 0 : 5;
  if(cell.x == 1)
    return cell.y == 0 ? 2 : 3;
  return -1;
}
// Face coordinates from the top left, to a direction, after the cube map
// selection rules of the GL and Vulkan specifications
vec3 shown_cube_dir(int face, vec2 tuv)
{
  float s = 2.0 * tuv.x - 1.0, t = 2.0 * tuv.y - 1.0;
  if(face == 0) return vec3(1.0, -t, -s);
  if(face == 1) return vec3(-1.0, -t, s);
  if(face == 2) return vec3(s, 1.0, t);
  if(face == 3) return vec3(s, -1.0, -t);
  if(face == 4) return vec3(s, -t, 1.0);
  return vec3(-s, -t, -1.0);
}
)_")
             .arg(n);
  }
  else
  {
    s += QStringLiteral(R"_(
int shown_layers() { return max(1, textureSize(%1, 0).z); }
ivec2 shown_tile_size() { return textureSize(%1, 0).xy; }
void shown_grid(out int tiles, out int cols, out int rows)
{
  tiles = min(shown_layers(), shown_max_tiles);
  cols = 1;
  while(cols * cols < tiles)
    cols++;
  rows = (tiles + cols - 1) / cols;
}
// Evenly spaced from the first to the last
int shown_tile(ivec2 cell, int tiles, int cols)
{
  int i = cell.y * cols + cell.x;
  if(i >= tiles)
    return -1;
  return tiles <= 1 ? 0 : (i * (shown_layers() - 1) + (tiles - 1) / 2) / (tiles - 1);
}
)_")
             .arg(n);
  }

  // Where a point of the shown image comes from: `tuv` is from the top left
  // of the tile as shown, then for a 3D texture or an array in the row order
  // of its layers, which is the one of every image (RowOrder.hpp). A cube
  // face is addressed by direction: the cross stays one whatever the order.
  s += QStringLiteral(R"_(
bool shown_locate(vec2 uv, int layout_, int index, bool flip, out int layer, out vec2 tuv)
{
  vec2 v = flip ? vec2(uv.x, 1.0 - uv.y) : uv;
  if(layout_ == 0)
  {
    layer = clamp(index, 0, shown_layers() - 1);
    tuv = v;
  }
  else
  {
    int tiles, cols, rows;
    shown_grid(tiles, cols, rows);
    vec2 g = v * vec2(cols, rows);
    ivec2 cell = clamp(ivec2(floor(g)), ivec2(0), ivec2(cols - 1, rows - 1));
    tuv = g - vec2(cell);
    layer = shown_tile(cell, tiles, cols);
    if(layer < 0)
      return false;
  }
)_");
  if(kind != TextureKind::Cube)
    s += QStringLiteral("  if(flip)\n    tuv.y = 1.0 - tuv.y;\n");
  s += QStringLiteral("  return true;\n}\n");

  QString read, fetch;
  switch(kind)
  {
    case TextureKind::Texture3D:
      read = QStringLiteral(
                 "textureLod(%1, vec3(tuv, (float(layer) + 0.5) / "
                 "float(shown_layers())), 0.0)")
                 .arg(n);
      fetch = QStringLiteral("texelFetch(%1, ivec3(t, layer), 0)").arg(n);
      break;
    case TextureKind::Array:
      read = QStringLiteral("textureLod(%1, vec3(tuv, float(layer)), 0.0)").arg(n);
      fetch = QStringLiteral("texelFetch(%1, ivec3(t, layer), 0)").arg(n);
      break;
    default:
      read = QStringLiteral("textureLod(%1, shown_cube_dir(layer, tuv), 0.0)").arg(n);
      // A cube map has no texelFetch: its texel centre, filtered
      fetch
          = QStringLiteral(
                "textureLod(%1, shown_cube_dir(layer, (vec2(t) + 0.5) / vec2(ts)), 0.0)")
                .arg(n);
      break;
  }
  s += QStringLiteral(R"_(
vec4 shownSample(vec2 uv, int layout_, int index, bool flip, out bool inside)
{
  int layer;
  vec2 tuv;
  inside = shown_locate(uv, layout_, index, flip, layer, tuv);
  if(!inside)
    return vec4(0.0);
  return %1;
}
vec4 shownTexel(ivec2 p, ivec2 size, int layout_, int index, bool flip, out bool inside)
{
  int layer;
  vec2 tuv;
  inside = shown_locate((vec2(p) + 0.5) / vec2(size), layout_, index, flip, layer, tuv);
  if(!inside)
    return vec4(0.0);
  ivec2 ts = shown_tile_size();
  ivec2 t = clamp(ivec2(tuv * vec2(ts)), ivec2(0), ts - 1);
  return %2;
}
)_")
           .arg(read, fetch);
  return s;
}
}
