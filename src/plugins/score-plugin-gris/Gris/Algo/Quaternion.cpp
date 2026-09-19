#include <Gris/Algo/Quaternion.hpp>

#include <cmath>

namespace Gris
{
Quaternion
getQuaternionFromEulerAngles(float yawParam, float pitchParam, float rollParam) noexcept
{
  // The formula used here is for spaces with the y axis being up.
  // Internally, SpatGRIS is z axis up. We need to mix up the angles for our
  // quaternion to match this.
  float const yawDeg{pitchParam};
  float const pitchDeg{yawParam};
  float const rollDeg{rollParam};

  float const yaw = yawDeg * radians_t::RADIAN_PER_DEGREE * -0.5f;
  float const pitch = pitchDeg * radians_t::RADIAN_PER_DEGREE * 0.5f;
  float const roll = rollDeg * radians_t::RADIAN_PER_DEGREE * 0.5f;

  float const sinYaw = std::sin(yaw);
  float const cosYaw = std::cos(yaw);
  float const sinPitch = std::sin(pitch);
  float const cosPitch = std::cos(pitch);
  float const sinRoll = std::sin(roll);
  float const cosRoll = std::cos(roll);
  float const cosPitchCosRoll = cosPitch * cosRoll;
  float const sinPitchSinRoll = sinPitch * sinRoll;

  // Z and Y are swapped and W negated, to match the left-handed coordinate
  // system SpatGRIS uses.
  return Quaternion{
      cosYaw * sinPitch * cosRoll - sinYaw * cosPitch * sinRoll, // X
      sinYaw * cosPitchCosRoll + cosYaw * sinPitchSinRoll,       // Z
      cosYaw * cosPitch * sinRoll + sinYaw * sinPitch * cosRoll, // Y
      -(cosYaw * cosPitchCosRoll - sinYaw * sinPitchSinRoll)     // -W
  };
}
} // namespace Gris
