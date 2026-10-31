#pragma once
// Read-back helpers of the multi-window output tests (MultiWindowCalibration,
// MultiWindowPhotometric, MultiWindowQuality).

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/FloatImageIO.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace score::test::gfx::multiwindow
{
struct Frame
{
  int width{}, height{};
  bool bgra{};
  QByteArray data;

  std::array<int, 3> px(int x, int y) const
  {
    const auto* p
        = reinterpret_cast<const uint8_t*>(data.constData()) + (y * width + x) * 4;
    if(bgra)
      return {p[2], p[1], p[0]};
    return {p[0], p[1], p[2]};
  }

  //! RGB of the pixel at fractional position (fx, fy) of the frame, 0..255.
  std::array<int, 3> at(double fx, double fy) const
  {
    return px(
        std::clamp(int(fx * width), 0, width - 1),
        std::clamp(int(fy * height), 0, height - 1));
  }
};

struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::string backend;
};

inline Gfx::OutputMapping single()
{
  Gfx::OutputMapping m;
  m.windowPosition = {20, 40};
  m.windowSize = {128, 64};
  return m;
}

inline bool grab(MultiWindowRig& rig, Frame& out)
{
  rig.render(3);
  rig.node->requestReadback(0);
  const auto& rb = rig.node->windowOutputs()[0].readback;
  for(int i = 0; i < 30 && rb->data.isEmpty(); i++)
    rig.render(1);
  if(rb->data.isEmpty())
    return false;
  out.width = rb->pixelSize.width();
  out.height = rb->pixelSize.height();
  out.bgra = rb->format == QRhiTexture::BGRA8;
  out.data = rb->data;
  return out.data.size() >= out.width * out.height * 4;
}

inline int u8(double v)
{
  return int(std::lround(std::clamp(v, 0., 1.) * 255.));
}

inline void writeGrey(const QString& path, int w, int h, float v)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = 1;
  img.data.assign(std::size_t(w) * h, v);
  REQUIRE(score::gfx::writePFM(path, img));
}

template <typename F>
Outcome runRig(
    score::gfx::GraphicsApi api, Gfx::OutputMapping mapping, F&& body,
    const QString& shader = QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"))
{
  Outcome o;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, {mapping}, shader))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    body(rig, o);
  });
  return o;
}
}
