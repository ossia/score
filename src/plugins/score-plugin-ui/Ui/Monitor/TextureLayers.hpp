#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <Gfx/Graph/TextureKind.hpp>

#include <QPointF>
#include <QStringList>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/TextureShapeMonitor.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>

/**
 * How the views of textures show one that holds several images, a 3D
 * texture, an array or a cube map: the controls that pick it, and what the
 * node tells the views of the texture it got.
 */
namespace Ui::Monitor
{
enum class Layout
{
  Single,
  Overview
};

struct LayoutControl : halp::combobox_t<"Layout", Layout>
{
  halp_meta(
      description,
      "For a 3D texture, an array or a cube map: one slice, layer or face, or "
      "all of them side by side")
  struct range
  {
    std::string_view values[2]{"Single", "Overview"};
    Layout init{Layout::Single};
  };
};

struct SliceControl : halp::hslider_f32<"Slice", halp::range{0., 1., 0.5}>
{
  halp_meta(
      description, "Depth of the slice of a 3D texture shown in Single, from 0 to 1")
};

struct LayerControl : halp::spinbox_i32<"Layer", halp::range{0, 2047, 0}>
{
  halp_meta(
      description,
      "Layer of an array shown in Single, or face of a cube map: 0 to 5 for "
      "+X, -X, +Y, -Y, +Z, -Z")
};

//! The node tells the views the texture it got.
inline void publish(TextureShapeMonitor& m, const score::gfx::TextureShape& s) noexcept
{
  m.shape.store(s.pack(), std::memory_order_relaxed);
}
inline score::gfx::TextureShape latestShape(const TextureShapeMonitor& m) noexcept
{
  return score::gfx::TextureShape::unpack(m.shape.load(std::memory_order_relaxed));
}

inline score::gfx::ShownTexture shownTexture(
    const score::gfx::TextureShape& shape, Layout layout, float slice,
    int layer) noexcept
{
  return score::gfx::ShownTexture::of(
      shape,
      layout == Layout::Overview ? score::gfx::TextureLayout::Overview
                                 : score::gfx::TextureLayout::Single,
      slice, layer);
}

//! What the controls Layout, Slice and Layer of `p`, which follow one
//! another, show of `shape`.
inline score::gfx::ShownTexture shownTexture(
    const Process::ProcessModel& p, const score::gfx::TextureShape& shape) noexcept
{
  std::size_t first = p.inlets().size();
  for(std::size_t i = 0; i < p.inlets().size(); i++)
    if(p.inlets()[i]->name() == QStringLiteral("Layout"))
    {
      first = i;
      break;
    }
  const auto* slice = controlInlet(p, first + 1);
  const auto* layer = controlInlet(p, first + 2);
  return shownTexture(
      shape, enumValue(controlInlet(p, first), Layout::Single, Layout::Overview),
      floatValue(slice, 0.5f), layer ? ossia::convert<int>(layer->value()) : 0);
}

/**
 * The readout of a texture of several images: what it is, and what a point
 * of the image shown, in [0, 1] from its top left, is in it.
 */
inline QStringList
describeShown(const score::gfx::ShownTexture& shown, std::optional<QPointF> at = {})
{
  QStringList lines;
  if(!shown.layered() || shown.shape.empty())
    return lines;
  lines << shown.shape.description();
  const auto pt
      = at ? shown.locate(at->x(), at->y()) : score::gfx::ShownTexture::Point{};
  if(pt.layer >= 0)
  {
    const int w = shown.shape.width, h = shown.shape.height;
    const int x = std::clamp(int(pt.u * w), 0, w - 1);
    const int y = std::clamp(int(pt.v * h), 0, h - 1);
    lines << QStringLiteral("%1: %2, %3").arg(shown.shape.layerName(pt.layer)).arg(x).arg(y);
  }
  else if(!shown.overview())
    lines << shown.shape.layerName(shown.index);
  return lines;
}
}
