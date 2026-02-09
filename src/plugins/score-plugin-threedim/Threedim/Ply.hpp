#pragma once
#include <Threedim/TinyObj.hpp>

namespace Threedim
{
std::vector<mesh> PlyFromFile(std::string_view filename, float_vec& data);

/**
 * Raw Gaussian Splat data loaded from a PLY file.
 *
 * Each splat is stored as 64 floats (256 bytes), laid out for direct GPU upload:
 *
 *   Offset (floats)  Content
 *   ──────────────────────────────────────
 *    0 ..  2         position (x, y, z)
 *    3 ..  5         normal (nx, ny, nz)
 *    6 ..  8         SH DC coefficients (f_dc_0, f_dc_1, f_dc_2)
 *    9 .. 53         SH rest coefficients (f_rest_0 .. f_rest_44)
 *   54               opacity (pre-sigmoid)
 *   55 .. 57         scale (log-space: scale_0, scale_1, scale_2)
 *   58 .. 61         rotation quaternion (rot_0=w, rot_1=x, rot_2=y, rot_3=z)
 *   62 .. 63         padding (zeros)
 *
 * A compute shader is expected to convert this to the compact rendering format.
 */
struct GaussianSplatData
{
  float_vec buffer;
  uint32_t splatCount{};

  static constexpr uint32_t floatsPerSplat = 64;
  static constexpr uint32_t bytesPerSplat = floatsPerSplat * sizeof(float); // 256
  static constexpr uint32_t maxSHRestCoeffs = 45;
  uint32_t shRestCount{};  // Actual number of f_rest coefficients found (0, 9, 24, or 45)
};

GaussianSplatData GaussianSplatsFromPly(std::string_view filename);
}
