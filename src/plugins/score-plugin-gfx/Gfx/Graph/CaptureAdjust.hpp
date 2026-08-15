#pragma once

/**
 * @file CaptureAdjust.hpp
 * @brief The corrections a capture node applies on its way out of the sensor.
 *
 * A raw sensor frame is not a picture. Demosaicing turns a mosaic into RGB and
 * stops there, deliberately, because everything after it is per-sensor: the
 * pedestal the sensor sits at, the gains that make grey grey under this light,
 * and the transfer curve that makes linear samples look right on a display.
 * Those belong to the camera, not to the reconstruction, which is why they are
 * a separate block rather than more constructor arguments on the decoder.
 *
 * Defaults are the identity. A pipeline that sets nothing renders exactly what
 * it rendered before this existed -- including the linear transfer curve, which
 * looks dark. Changing that default would silently restyle every existing
 * project, so the correction is offered rather than imposed.
 *
 * Published by a control callback on whatever thread wrote it, consumed by the
 * render thread. A generation counter carries the handoff: the renderer reads
 * one atomic per frame and only takes the lock when something actually moved,
 * so dragging a slider never stalls rendering and rendering never stalls the UI.
 */

#include <Gfx/Graph/Scale.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace score::gfx
{

/// Sensor-side corrections, in the order the shader applies them.
struct CaptureAdjust
{
  /// Subtracted before anything else, normalised to the sample range. A sensor
  /// with a pedestal reads this much with the lens capped, and leaving it in
  /// washes the blacks and skews every gain applied after it.
  float blackLevel[3]{0.f, 0.f, 0.f};

  /// Per-channel gain. Bayer has twice as many green sites as red or blue, so
  /// an uncorrected frame reads green; this is what makes grey grey.
  float whiteBalance[3]{1.f, 1.f, 1.f};

  /// Linear multiplier, after white balance.
  float exposure{1.f};

  /// Encoding exponent: the shader raises to 1/gamma. 1 leaves the signal
  /// linear, which is the pre-existing behaviour; 2.2 approximates sRGB.
  float gamma{1.f};

  /// 0 collapses to luma, 1 leaves it alone.
  float saturation{1.f};

  /// How the frame is fitted to the viewport it is drawn into.
  ///
  /// Stretch, not Original, because Stretch is what a scale of (1,1) means and
  /// (1,1) is what this node drew with before the mode existed. Original scales
  /// the quad by texture/viewport, so defaulting to it would have zoomed and
  /// cropped every capture already in use.
  ScaleMode scaleMode{ScaleMode::Stretch};

  friend bool operator==(const CaptureAdjust&, const CaptureAdjust&) noexcept
      = default;
};

/**
 * @brief Cross-thread handoff for CaptureAdjust.
 *
 * Not lock-free, and deliberately: a mutex taken only when the generation
 * changed costs the render thread one relaxed load per frame in the common
 * case, and reading the fields individually as atomics would let the renderer
 * see half of one setting and half of the next -- a visible colour flash while
 * dragging two sliders.
 */
class CaptureAdjustSlot
{
public:
  /// Writer side. Any thread.
  void set(const CaptureAdjust& v)
  {
    {
      std::lock_guard lk{m_mutex};
      if(m_value == v)
        return;
      m_value = v;
    }
    m_generation.fetch_add(1, std::memory_order_release);
  }

  /// Reader side. Returns true when @p out was updated, i.e. only when
  /// something changed since the last call with this @p lastSeen.
  bool poll(CaptureAdjust& out, std::uint32_t& lastSeen) const
  {
    const auto gen = m_generation.load(std::memory_order_acquire);
    if(gen == lastSeen)
      return false;
    {
      std::lock_guard lk{m_mutex};
      out = m_value;
    }
    lastSeen = gen;
    return true;
  }

  CaptureAdjust value() const
  {
    std::lock_guard lk{m_mutex};
    return m_value;
  }

  std::uint32_t generation() const noexcept
  {
    return m_generation.load(std::memory_order_acquire);
  }

private:
  mutable std::mutex m_mutex;
  CaptureAdjust m_value;
  std::atomic<std::uint32_t> m_generation{0};
};

#pragma pack(push, 1)
/**
 * @brief The capture path's material block: the shared one, plus corrections.
 *
 * A superset of VideoMaterialUBO rather than a change to it. The first four
 * floats are laid out identically, so a decoder whose shader declares only the
 * short block reads the same buffer correctly, and the decoders that do want
 * the corrections declare the long one. Extending the shared struct instead
 * would have required every renderer that allocates it -- including the two
 * video paths -- to grow its buffer in lockstep or bind one too small for the
 * block its shader declares.
 *
 * std140: two vec2 fill the first 16 bytes, then each vec4 lands on its own
 * 16-byte boundary.
 */
struct CaptureMaterialUBO
{
  float scale[2]{1.f, 1.f};
  float textureSize[2]{1.f, 1.f};
  float blackLevel[4]{0.f, 0.f, 0.f, 0.f};
  float whiteBalance[4]{1.f, 1.f, 1.f, 0.f};
  /// exposure, gamma, saturation, unused
  float params[4]{1.f, 1.f, 1.f, 0.f};
};
#pragma pack(pop)

static_assert(sizeof(CaptureMaterialUBO) == 64);
static_assert(offsetof(CaptureMaterialUBO, blackLevel) == 16);
static_assert(offsetof(CaptureMaterialUBO, whiteBalance) == 32);
static_assert(offsetof(CaptureMaterialUBO, params) == 48);

/// Fills the correction half of @p ubo. The geometry half is the caller's.
inline void applyTo(const CaptureAdjust& a, CaptureMaterialUBO& ubo) noexcept
{
  for(int i = 0; i < 3; ++i)
  {
    ubo.blackLevel[i] = a.blackLevel[i];
    ubo.whiteBalance[i] = a.whiteBalance[i];
  }
  ubo.params[0] = a.exposure;
  ubo.params[1] = a.gamma;
  ubo.params[2] = a.saturation;
}

/**
 * @brief The corrections, on the CPU, in the same order the shader applies them.
 *
 * This is the specification: CaptureAdjustGLSL.hpp implements it on the GPU,
 * and a test can only check the two agree if one of them is checkable. Keep
 * them in step -- a divergence shows up as the picture changing when the
 * capture rung changes, which looks like a capture bug.
 */
inline void adjustCaptureReference(const CaptureAdjust& a, float rgb[3]) noexcept
{
  constexpr float eps = 1.f / 65535.f;

  for(int i = 0; i < 3; ++i)
  {
    const float bl = a.blackLevel[i];
    float c = (rgb[i] - bl) / std::max(eps, 1.f - bl);
    c *= a.whiteBalance[i];
    c *= a.exposure;
    rgb[i] = std::max(0.f, c);
  }

  const float l
      = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
  for(int i = 0; i < 3; ++i)
    rgb[i] = std::max(0.f, l + (rgb[i] - l) * a.saturation);

  const float inv = 1.f / std::max(a.gamma, eps);
  for(int i = 0; i < 3; ++i)
    rgb[i] = std::clamp(std::pow(rgb[i], inv), 0.f, 1.f);
}

} // namespace score::gfx
