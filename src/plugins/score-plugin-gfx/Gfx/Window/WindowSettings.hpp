#pragma once
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

#include <array>

namespace Gfx
{

struct WindowOutputSettings
{
  int width{};
  int height{};
  double rate{};
  bool viewportSize{};
  bool vsync{};
};

enum class WindowMode : int
{
  Single = 0,
  Background = 1,
  MultiWindow = 2
};

enum class SwapchainFlag : int
{
  NoFlag = 0,
  sRGB = (1 << 2)
};

// Matches QRhiSwapchainFormat
enum class SwapchainFormat : int
{
  SDR,
  HDRExtendedSrgbLinear,
  HDR10,
  HDRExtendedDisplayP3Linear
};


struct EdgeBlend
{
  float width{0.0f}; // Blend width in UV space (0.0 = no blend, 0.15 = 15% of output)
  float gamma{2.2f}; // Blend curve exponent (1.0 = linear, 2.2 = typical projector)
};

struct CornerWarp
{
  QPointF topLeft{0.0, 0.0};
  QPointF topRight{1.0, 0.0};
  QPointF bottomLeft{0.0, 1.0};
  QPointF bottomRight{1.0, 1.0};

  bool isIdentity() const noexcept
  {
    constexpr double eps = 1e-6;
    return qAbs(topLeft.x()) < eps && qAbs(topLeft.y()) < eps
           && qAbs(topRight.x() - 1.0) < eps && qAbs(topRight.y()) < eps
           && qAbs(bottomLeft.x()) < eps && qAbs(bottomLeft.y() - 1.0) < eps
           && qAbs(bottomRight.x() - 1.0) < eps && qAbs(bottomRight.y() - 1.0) < eps;
  }
};

struct OutputColor
{
  std::array<float, 3> gain{1.f, 1.f, 1.f};
  std::array<float, 3> offset{0.f, 0.f, 0.f};
  //! Applied last: out = pow(c, 1 / gamma).
  float gamma{1.f};
  //! Decode the content with a 2.2 power before the blend ramps, alpha and
  //! beta maps, re-encode after.
  bool linearBlend{false};

  bool isIdentity() const noexcept
  {
    return gain == std::array<float, 3>{1.f, 1.f, 1.f}
           && offset == std::array<float, 3>{0.f, 0.f, 0.f} && gamma == 1.f
           && !linearBlend;
  }
};

//! Calibration files of an output, in its screen space (v = 0 at the top).
struct OutputMaps
{
  //! UV lookup per output pixel, like alpha and beta (xy = source UV in the
  //! output's source rect, w = valid); rotation, mirror and corner warp then
  //! move the UV it gives.
  QString warp;
  //! Blend weight per RGB channel (a grey map broadcasts), multiplied with the edge ramps.
  QString alpha;
  //! Black level per RGB channel, added after the alpha.
  QString beta;
  //! Colour lookup table (.cube, Adobe Cube LUT 1.0), applied last.
  QString lut;
  //! Measured projector response (.response.json or 1D .cube): blending then
  //! runs in linear light, and its inverse is the encode back to projector codes.
  QString response;
  //! Radiometric compensation (.comp.json): per pixel 3x3 matrix and offset
  //! applied to the linear content before the alpha.
  QString compensation;
  //! name.json of the calibration the files above follow (CalibrationSet.hpp).
  QString calibration;

  bool empty() const noexcept
  {
    return warp.isEmpty() && alpha.isEmpty() && beta.isEmpty() && lut.isEmpty()
           && response.isEmpty() && compensation.isEmpty();
  }
};

//! How the output reads the canvas.
enum class OutputSampling : int
{
  //! Anisotropic when a warp map or a corner warp is active, else Linear.
  Auto = 0,
  Linear = 1,
  //! Trilinear, at the level of detail of the warp's screen-space Jacobian.
  Mipmapped = 2,
  //! Up to maxAnisotropy trilinear taps along the footprint's major axis.
  Anisotropic = 3
};

enum class WarpInterpolation : int
{
  //! Bilinear, weighted by the valid flag of each texel.
  Bilinear = 0,
  //! Catmull-Rom where the 16 texels are valid, else Bilinear.
  Bicubic = 1
};

enum class OutputDither : int
{
  //! On for an 8 or 10-bit output when blending, a map or the colour stage is active.
  Auto = 0,
  Off = 1,
  On = 2
};

struct OutputQuality
{
  OutputSampling sampling{OutputSampling::Auto};
  WarpInterpolation warpInterpolation{WarpInterpolation::Bicubic};
  OutputDither dither{OutputDither::Auto};
  int maxAnisotropy{8};

  bool isDefault() const noexcept
  {
    return sampling == OutputSampling::Auto
           && warpInterpolation == WarpInterpolation::Bicubic
           && dither == OutputDither::Auto && maxAnisotropy == 8;
  }
};

enum class OutputLockMode : int
{
  Free = 0,       // No constraints
  AspectRatio = 1, // Output window preserves input section aspect ratio
  OneToOne = 2,    // Output window size = input source rect pixels (1:1)
  FullLock = 3     // Cannot move or resize in graphics scenes
};

//! Where a new output window goes when nothing says otherwise.
static const constexpr QPoint defaultWindowPosition{100, 100};

/**
 * @brief Where to put the window of an output whose source starts at \p p.
 *
 * The desktop origin is a bad place for a window: it is hard to grab by its
 * title bar on Windows, and a tiling window manager may not map it at all --
 * on i3 the first output window stayed invisible until it was moved by one
 * pixel. Only that exact spot is moved; anywhere else is left alone.
 */
inline QPoint windowPositionForSource(QPoint p) noexcept
{
  return p.isNull() ? defaultWindowPosition : p;
}

struct OutputMapping
{
  QRectF sourceRect{0.0, 0.0, 1.0, 1.0}; // UV coords in input texture
  int screenIndex{-1};                   // -1 = default screen
  //! Not the origin: a window there is hard to grab by its title bar on
  //! Windows, and a tiling window manager may not map it at all (on i3 the
  //! first output window stayed invisible until it was moved by one pixel).
  QPoint windowPosition{defaultWindowPosition};
  QSize windowSize{1280, 720};
  bool fullscreen{false};

  // Soft-edge blending per side
  EdgeBlend blendLeft;
  EdgeBlend blendRight;
  EdgeBlend blendTop;
  EdgeBlend blendBottom;

  // 4-corner perspective warp (output UV space)
  CornerWarp cornerWarp;

  OutputLockMode lockMode{OutputLockMode::Free};

  int rotation{0};     // 0, 90, 180, 270 degrees clockwise
  bool mirrorX{false}; // Flip output horizontally (after rotation)
  bool mirrorY{false}; // Flip output vertically (after rotation)

  OutputMaps maps;
  OutputColor color;
  OutputQuality quality;
};

}
