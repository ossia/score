#pragma once

/* Ported from StructGRIS Data/Quaternion.{hpp,cpp} (GRIS / SAT, GPLv3),
 * unchanged apart from the namespace and the radians constant. Used to place
 * the speakers of a rotated speaker group. */

#include <Gris/Algo/Types.hpp>

#include <array>

namespace Gris
{
using Quaternion = std::array<float, 4>;

[[nodiscard]] Quaternion
getQuaternionFromEulerAngles(float yawParam, float pitchParam, float rollParam) noexcept;

[[nodiscard]] constexpr Quaternion quatMult(Quaternion const& a, Quaternion const& b) noexcept
{
  Quaternion result{};
  result[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  result[1] = a[0] * b[1] + a[1] * b[0] - a[2] * b[3] + a[3] * b[2];
  result[2] = a[0] * b[2] + a[1] * b[3] + a[2] * b[0] - a[3] * b[1];
  result[3] = a[0] * b[3] - a[1] * b[2] + a[2] * b[1] + a[3] * b[0];
  return result;
}

[[nodiscard]] constexpr Quaternion quatInv(Quaternion const& a) noexcept
{
  return {a[0], -a[1], -a[2], -a[3]};
}

[[nodiscard]] constexpr std::array<float, 3>
quatRotation(std::array<float, 3> const& xyz, Quaternion const& rotQuat) noexcept
{
  Quaternion const xyzQuat{0, xyz[0], xyz[1], xyz[2]};
  auto const resultQuat = quatMult(quatMult(quatInv(rotQuat), xyzQuat), rotQuat);
  return {resultQuat[1], resultQuat[2], resultQuat[3]};
}
} // namespace Gris
