#pragma once

/* Minimal, JUCE-free replacements for the StructGRIS types that the GRIS
 * spatialisation algorithms (VBAP, MBAP) actually use.
 *
 * Only the surface touched by Implementations/sg_vbap.cpp and sg_mbap.cpp is
 * reproduced here -- see the porting plan, section 5b.3: the two gain
 * computers read exactly three fields from a source (position, azimuth span,
 * zenith span) and never touch the rest of the SpatGRIS data model.
 *
 * Original data model: StructGRIS, GRIS / SAT, GPLv3.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace Gris
{
//==============================================================================
inline constexpr auto MAX_NUM_SOURCES = 256;
inline constexpr auto MAX_NUM_SPEAKERS = 256;

inline constexpr auto NORMAL_RADIUS = 1.0f;
inline constexpr auto MBAP_EXTENDED_RADIUS = 1.6666667f;
inline constexpr auto EXTRA_DISTANCE = MBAP_EXTENDED_RADIUS - NORMAL_RADIUS;
inline constexpr auto SQRT3 = 1.7320508075688772935274463415059f;
inline constexpr auto SMALL_GAIN = 0.0000000001f;

inline constexpr float PI_F = 3.14159265358979323846f;
inline constexpr float HALF_PI_F = PI_F / 2.f;
inline constexpr float TWO_PI_F = PI_F * 2.f;

//==============================================================================
/** Checked narrowing conversion. Mirrors StructGRIS's narrow<>(). */
template <typename To, typename From>
[[nodiscard]] constexpr To narrow(From const value) noexcept
{
  return static_cast<To>(value);
}

//==============================================================================
class degrees_t;

/** An angle in radians. Replaces gris::radians_t (StrongFloat<RadiansT>). */
class radians_t
{
public:
  using type = float;
  static constexpr type RADIAN_PER_DEGREE{TWO_PI_F / 360.f};

  constexpr radians_t() = default;
  explicit constexpr radians_t(type value) noexcept
      : m_value{value}
  {
  }
  explicit constexpr radians_t(degrees_t const& degrees) noexcept;

  [[nodiscard]] constexpr type get() const noexcept { return m_value; }
  [[nodiscard]] constexpr type getAsRadians() const noexcept { return m_value; }
  [[nodiscard]] constexpr type getAsDegrees() const noexcept;
  [[nodiscard]] constexpr degrees_t toDegrees() const noexcept;

  [[nodiscard]] constexpr radians_t operator+(radians_t o) const noexcept
  {
    return radians_t{m_value + o.m_value};
  }
  [[nodiscard]] constexpr radians_t operator-(radians_t o) const noexcept
  {
    return radians_t{m_value - o.m_value};
  }
  [[nodiscard]] constexpr radians_t operator-() const noexcept
  {
    return radians_t{-m_value};
  }
  [[nodiscard]] constexpr radians_t operator*(type s) const noexcept
  {
    return radians_t{m_value * s};
  }
  [[nodiscard]] constexpr radians_t operator/(type s) const noexcept
  {
    return radians_t{m_value / s};
  }
  [[nodiscard]] constexpr type operator/(radians_t o) const noexcept
  {
    return m_value / o.m_value;
  }
  constexpr radians_t& operator+=(radians_t o) noexcept
  {
    m_value += o.m_value;
    return *this;
  }
  constexpr radians_t& operator-=(radians_t o) noexcept
  {
    m_value -= o.m_value;
    return *this;
  }

  [[nodiscard]] constexpr bool operator==(radians_t o) const noexcept
  {
    return m_value == o.m_value;
  }
  [[nodiscard]] constexpr bool operator!=(radians_t o) const noexcept
  {
    return m_value != o.m_value;
  }
  [[nodiscard]] constexpr bool operator<(radians_t o) const noexcept
  {
    return m_value < o.m_value;
  }
  [[nodiscard]] constexpr bool operator<=(radians_t o) const noexcept
  {
    return m_value <= o.m_value;
  }
  [[nodiscard]] constexpr bool operator>(radians_t o) const noexcept
  {
    return m_value > o.m_value;
  }
  [[nodiscard]] constexpr bool operator>=(radians_t o) const noexcept
  {
    return m_value >= o.m_value;
  }

  /** Wraps into ]-pi, pi]. StructGRIS: centeredAroundZero(twoPi). */
  [[nodiscard]] constexpr radians_t balanced() const noexcept
  {
    return centeredAroundZero(TWO_PI_F);
  }
  [[nodiscard]] constexpr radians_t centered() const noexcept
  {
    return centeredAroundZero(TWO_PI_F);
  }
  [[nodiscard]] constexpr radians_t madePositive() const noexcept
  {
    return radians_t{m_value < 0 ? m_value + TWO_PI_F : m_value};
  }

  [[nodiscard]] static radians_t angleOf(float x, float y) noexcept
  {
    if(std::fpclassify(x) == FP_ZERO && std::fpclassify(y) == FP_ZERO)
      return radians_t{};
    return radians_t{std::atan2(y, x)};
  }

private:
  [[nodiscard]] constexpr radians_t centeredAroundZero(type period) const noexcept
  {
    auto const halfPeriod = period / 2.f;
    auto value = m_value;
    while(value > halfPeriod)
      value -= period;
    while(value <= -halfPeriod)
      value += period;
    return radians_t{value};
  }

  type m_value{};
};

//==============================================================================
/** An angle in degrees. Replaces gris::degrees_t. */
class degrees_t
{
public:
  using type = float;
  static constexpr type DEGREE_PER_RADIAN{360.f / TWO_PI_F};

  constexpr degrees_t() = default;
  explicit constexpr degrees_t(type value) noexcept
      : m_value{value}
  {
  }
  explicit constexpr degrees_t(radians_t const& radians) noexcept
      : m_value{radians.get() * DEGREE_PER_RADIAN}
  {
  }

  [[nodiscard]] constexpr type get() const noexcept { return m_value; }
  [[nodiscard]] constexpr radians_t toRadians() const noexcept
  {
    return radians_t{m_value * radians_t::RADIAN_PER_DEGREE};
  }
  [[nodiscard]] constexpr operator radians_t() const noexcept { return toRadians(); }

  [[nodiscard]] constexpr degrees_t operator+(degrees_t o) const noexcept
  {
    return degrees_t{m_value + o.m_value};
  }
  [[nodiscard]] constexpr degrees_t operator-(degrees_t o) const noexcept
  {
    return degrees_t{m_value - o.m_value};
  }
  [[nodiscard]] constexpr degrees_t operator-() const noexcept
  {
    return degrees_t{-m_value};
  }
  [[nodiscard]] constexpr degrees_t operator*(type s) const noexcept
  {
    return degrees_t{m_value * s};
  }
  [[nodiscard]] constexpr degrees_t operator/(type s) const noexcept
  {
    return degrees_t{m_value / s};
  }
  [[nodiscard]] constexpr bool operator==(degrees_t o) const noexcept
  {
    return m_value == o.m_value;
  }
  [[nodiscard]] constexpr bool operator<(degrees_t o) const noexcept
  {
    return m_value < o.m_value;
  }
  [[nodiscard]] constexpr bool operator>(degrees_t o) const noexcept
  {
    return m_value > o.m_value;
  }

private:
  type m_value{};
};

constexpr radians_t::radians_t(degrees_t const& degrees) noexcept
    : m_value{degrees.get() * RADIAN_PER_DEGREE}
{
}
constexpr radians_t::type radians_t::getAsDegrees() const noexcept
{
  return m_value * degrees_t::DEGREE_PER_RADIAN;
}
constexpr degrees_t radians_t::toDegrees() const noexcept
{
  return degrees_t{m_value * degrees_t::DEGREE_PER_RADIAN};
}

inline constexpr radians_t QUARTER_PI{PI_F / 4.f};
inline constexpr radians_t HALF_PI{HALF_PI_F};
inline constexpr radians_t PI{PI_F};
inline constexpr radians_t TWO_PI{TWO_PI_F};

//==============================================================================
/** 1-based physical output index. Replaces gris::output_patch_t. */
class output_patch_t
{
public:
  using type = int;

  constexpr output_patch_t() = default;
  explicit constexpr output_patch_t(type value) noexcept
      : m_value{value}
  {
  }

  [[nodiscard]] constexpr type get() const noexcept { return m_value; }

  [[nodiscard]] constexpr bool operator==(output_patch_t o) const noexcept
  {
    return m_value == o.m_value;
  }
  [[nodiscard]] constexpr bool operator!=(output_patch_t o) const noexcept
  {
    return m_value != o.m_value;
  }
  [[nodiscard]] constexpr bool operator<(output_patch_t o) const noexcept
  {
    return m_value < o.m_value;
  }
  [[nodiscard]] constexpr bool operator<=(output_patch_t o) const noexcept
  {
    return m_value <= o.m_value;
  }
  [[nodiscard]] constexpr bool operator>(output_patch_t o) const noexcept
  {
    return m_value > o.m_value;
  }
  [[nodiscard]] constexpr bool operator>=(output_patch_t o) const noexcept
  {
    return m_value >= o.m_value;
  }

  constexpr output_patch_t& operator++() noexcept
  {
    ++m_value;
    return *this;
  }

private:
  type m_value{};
};

//==============================================================================
/** 1-based source index. Replaces gris::source_index_t. */
class source_index_t
{
public:
  using type = int;

  constexpr source_index_t() = default;
  explicit constexpr source_index_t(type value) noexcept
      : m_value{value}
  {
  }
  [[nodiscard]] constexpr type get() const noexcept { return m_value; }
  [[nodiscard]] constexpr bool operator==(source_index_t o) const noexcept
  {
    return m_value == o.m_value;
  }
  [[nodiscard]] constexpr bool operator<(source_index_t o) const noexcept
  {
    return m_value < o.m_value;
  }

private:
  type m_value{};
};

//==============================================================================
struct PolarVector;

/** Replaces gris::CartesianVector. */
struct CartesianVector
{
  float x{};
  float y{};
  float z{};

  constexpr CartesianVector() = default;
  constexpr CartesianVector(float newX, float newY, float newZ) noexcept
      : x{newX}
      , y{newY}
      , z{newZ}
  {
  }
  explicit CartesianVector(PolarVector const& polar) noexcept;

  [[nodiscard]] constexpr bool operator==(CartesianVector const& o) const noexcept
  {
    return x == o.x && y == o.y && z == o.z;
  }
  [[nodiscard]] constexpr bool operator!=(CartesianVector const& o) const noexcept
  {
    return !(*this == o);
  }
  [[nodiscard]] constexpr CartesianVector operator+(CartesianVector const& o) const noexcept
  {
    return {x + o.x, y + o.y, z + o.z};
  }
  [[nodiscard]] constexpr CartesianVector operator-(CartesianVector const& o) const noexcept
  {
    return {x - o.x, y - o.y, z - o.z};
  }
  [[nodiscard]] constexpr CartesianVector operator*(float s) const noexcept
  {
    return {x * s, y * s, z * s};
  }
  [[nodiscard]] constexpr CartesianVector operator/(float s) const noexcept
  {
    return {x / s, y / s, z / s};
  }
  [[nodiscard]] constexpr CartesianVector operator-() const noexcept
  {
    return {-x, -y, -z};
  }

  [[nodiscard]] constexpr CartesianVector withX(float v) const noexcept { return {v, y, z}; }
  [[nodiscard]] constexpr CartesianVector withY(float v) const noexcept { return {x, v, z}; }
  [[nodiscard]] constexpr CartesianVector withZ(float v) const noexcept { return {x, y, v}; }
  [[nodiscard]] constexpr CartesianVector translatedX(float d) const noexcept
  {
    return {x + d, y, z};
  }
  [[nodiscard]] constexpr CartesianVector translatedY(float d) const noexcept
  {
    return {x, y + d, z};
  }
  [[nodiscard]] constexpr CartesianVector translatedZ(float d) const noexcept
  {
    return {x, y, z + d};
  }

  [[nodiscard]] float length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
  [[nodiscard]] constexpr float dotProduct(CartesianVector const& o) const noexcept
  {
    return x * o.x + y * o.y + z * o.z;
  }
  [[nodiscard]] constexpr CartesianVector crossProduct(CartesianVector const& o) const noexcept
  {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }
  [[nodiscard]] CartesianVector normalized() const noexcept
  {
    auto const l = length();
    return l > 0.f ? *this / l : *this;
  }
  [[nodiscard]] float angleWith(CartesianVector const& o) const noexcept
  {
    auto inner = dotProduct(o) / std::sqrt(length() * o.length());
    inner = std::clamp(inner, -1.f, 1.f);
    return std::abs(std::acos(inner));
  }
  /** Clips to the MBAP extended cube. */
  [[nodiscard]] constexpr CartesianVector clampedToFarField() const noexcept
  {
    return {
        std::clamp(x, -MBAP_EXTENDED_RADIUS, MBAP_EXTENDED_RADIUS),
        std::clamp(y, -MBAP_EXTENDED_RADIUS, MBAP_EXTENDED_RADIUS),
        std::clamp(z, -MBAP_EXTENDED_RADIUS, MBAP_EXTENDED_RADIUS)};
  }
};

//==============================================================================
/** Replaces gris::PolarVector. */
struct PolarVector
{
  radians_t azimuth{};
  radians_t elevation{};
  float length{};

  constexpr PolarVector() = default;
  constexpr PolarVector(radians_t a, radians_t e, float l) noexcept
      : azimuth{a}
      , elevation{e}
      , length{l}
  {
  }
  explicit PolarVector(CartesianVector const& c) noexcept;

  [[nodiscard]] constexpr bool operator==(PolarVector const& o) const noexcept
  {
    return azimuth == o.azimuth && elevation == o.elevation && length == o.length;
  }
  [[nodiscard]] constexpr PolarVector withAzimuth(radians_t a) const noexcept
  {
    return {a, elevation, length};
  }
  [[nodiscard]] constexpr PolarVector withBalancedAzimuth(radians_t a) const noexcept
  {
    return {a.balanced(), elevation, length};
  }
  [[nodiscard]] constexpr PolarVector withElevation(radians_t e) const noexcept
  {
    return {azimuth, e, length};
  }
  [[nodiscard]] constexpr PolarVector withClippedElevation(radians_t e) const noexcept
  {
    return {azimuth, radians_t{std::clamp(e.get(), 0.f, HALF_PI_F)}, length};
  }
  [[nodiscard]] constexpr PolarVector withRadius(float r) const noexcept
  {
    return {azimuth, elevation, r};
  }
  [[nodiscard]] constexpr PolarVector withPositiveRadius(float r) const noexcept
  {
    return {azimuth, elevation, std::max(r, 0.f)};
  }
  [[nodiscard]] constexpr PolarVector normalized() const noexcept
  {
    return {azimuth, elevation, 1.f};
  }
  [[nodiscard]] constexpr PolarVector rotatedAzimuth(radians_t d) const noexcept
  {
    return withAzimuth(azimuth + d);
  }
  [[nodiscard]] constexpr PolarVector rotatedBalancedAzimuth(radians_t d) const noexcept
  {
    return withBalancedAzimuth(azimuth + d);
  }
  [[nodiscard]] constexpr PolarVector elevated(radians_t d) const noexcept
  {
    return withElevation(elevation + d);
  }
  [[nodiscard]] constexpr PolarVector elevatedClipped(radians_t d) const noexcept
  {
    return withClippedElevation(elevation + d);
  }
  [[nodiscard]] constexpr PolarVector pushed(float d) const noexcept
  {
    return withRadius(length + d);
  }
  [[nodiscard]] constexpr PolarVector pushedWithPositiveRadius(float d) const noexcept
  {
    return withPositiveRadius(length + d);
  }
};

//==============================================================================
inline CartesianVector::CartesianVector(PolarVector const& p) noexcept
{
  // Copied from StructGRIS sg_CartesianVector.cpp so that the conversion is
  // bit-identical to SpatGRIS.
  auto const cosEl = std::cos(p.elevation.get());
  x = p.length * std::cos(p.azimuth.get()) * cosEl;
  y = p.length * std::sin(p.azimuth.get()) * cosEl;
  z = p.length * std::sin(p.elevation.get());
}

inline PolarVector::PolarVector(CartesianVector const& c) noexcept
{
  length = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  if(length == 0.f)
  {
    azimuth = radians_t{};
    elevation = radians_t{};
    return;
  }
  azimuth = radians_t::angleOf(c.x, c.y);
  elevation = radians_t{std::asin(std::clamp(c.z / length, -1.f, 1.f))};
}

//==============================================================================
/** Replaces gris::Position: a point kept in both coordinate systems. */
class Position
{
public:
  Position() = default;
  explicit Position(PolarVector const& polar) noexcept
      : m_polar{polar}
      , m_cartesian{CartesianVector{polar}}
  {
  }
  explicit Position(CartesianVector const& cartesian) noexcept
      : m_polar{PolarVector{cartesian}}
      , m_cartesian{cartesian}
  {
  }

  [[nodiscard]] constexpr PolarVector const& getPolar() const noexcept { return m_polar; }
  [[nodiscard]] constexpr CartesianVector const& getCartesian() const noexcept
  {
    return m_cartesian;
  }

  [[nodiscard]] bool operator==(Position const& o) const noexcept
  {
    return m_cartesian == o.m_cartesian;
  }
  [[nodiscard]] bool operator!=(Position const& o) const noexcept { return !(*this == o); }

  Position& operator=(PolarVector const& p) noexcept
  {
    m_polar = p;
    m_cartesian = CartesianVector{p};
    return *this;
  }
  Position& operator=(CartesianVector const& c) noexcept
  {
    m_cartesian = c;
    m_polar = PolarVector{c};
    return *this;
  }

  [[nodiscard]] Position withAzimuth(radians_t a) const noexcept
  {
    return Position{m_polar.withAzimuth(a)};
  }
  [[nodiscard]] Position withBalancedAzimuth(radians_t a) const noexcept
  {
    return Position{m_polar.withBalancedAzimuth(a)};
  }
  [[nodiscard]] Position withElevation(radians_t e) const noexcept
  {
    return Position{m_polar.withElevation(e)};
  }
  [[nodiscard]] Position withClippedElevation(radians_t e) const noexcept
  {
    return Position{m_polar.withClippedElevation(e)};
  }
  [[nodiscard]] Position withRadius(float r) const noexcept
  {
    return Position{m_polar.withRadius(r)};
  }
  [[nodiscard]] Position withPositiveRadius(float r) const noexcept
  {
    return Position{m_polar.withPositiveRadius(r)};
  }
  [[nodiscard]] Position withX(float v) const noexcept
  {
    return Position{m_cartesian.withX(v)};
  }
  [[nodiscard]] Position withY(float v) const noexcept
  {
    return Position{m_cartesian.withY(v)};
  }
  [[nodiscard]] Position withZ(float v) const noexcept
  {
    return Position{m_cartesian.withZ(v)};
  }
  [[nodiscard]] Position rotatedAzimuth(radians_t d) const noexcept
  {
    return Position{m_polar.rotatedAzimuth(d)};
  }
  [[nodiscard]] Position rotatedBalancedAzimuth(radians_t d) const noexcept
  {
    return Position{m_polar.rotatedBalancedAzimuth(d)};
  }
  [[nodiscard]] Position elevated(radians_t d) const noexcept
  {
    return Position{m_polar.elevated(d)};
  }
  [[nodiscard]] Position elevatedClipped(radians_t d) const noexcept
  {
    return Position{m_polar.elevatedClipped(d)};
  }
  [[nodiscard]] Position pushed(float d) const noexcept { return Position{m_polar.pushed(d)}; }
  [[nodiscard]] Position pushedWithPositiveRadius(float d) const noexcept
  {
    return Position{m_polar.pushedWithPositiveRadius(d)};
  }
  [[nodiscard]] Position normalized() const noexcept { return Position{m_polar.normalized()}; }
  [[nodiscard]] Position translatedX(float d) const noexcept
  {
    return Position{m_cartesian.translatedX(d)};
  }
  [[nodiscard]] Position translatedY(float d) const noexcept
  {
    return Position{m_cartesian.translatedY(d)};
  }
  [[nodiscard]] Position translatedZ(float d) const noexcept
  {
    return Position{m_cartesian.translatedZ(d)};
  }

private:
  PolarVector m_polar{};
  CartesianVector m_cartesian{};
};

//==============================================================================
/** Output patches are 1-based and bounded by MAX_NUM_SPEAKERS.
 *  Replaces gris::LEGAL_OUTPUT_PATCH_RANGE. */
[[nodiscard]] constexpr bool isLegalOutputPatch(output_patch_t patch) noexcept
{
  return patch.get() >= 1 && patch.get() <= MAX_NUM_SPEAKERS;
}

//==============================================================================
/** A VBAP speaker triplet, for the (display-only) triangulation. */
struct Triplet
{
  output_patch_t id1{};
  output_patch_t id2{};
  output_patch_t id3{};
};

//==============================================================================
/** Per-speaker gains for one source, indexed by output patch. */
class SpeakersSpatGains
{
public:
  [[nodiscard]] float& operator[](output_patch_t patch) noexcept
  {
    return m_gains[index(patch)];
  }
  [[nodiscard]] float const& operator[](output_patch_t patch) const noexcept
  {
    return m_gains[index(patch)];
  }

  void fill(float value) noexcept { m_gains.fill(value); }
  [[nodiscard]] float* data() noexcept { return m_gains.data(); }
  [[nodiscard]] float const* data() const noexcept { return m_gains.data(); }
  [[nodiscard]] auto cbegin() const noexcept { return m_gains.cbegin(); }
  [[nodiscard]] auto cend() const noexcept { return m_gains.cend(); }
  [[nodiscard]] static constexpr std::size_t size() noexcept { return MAX_NUM_SPEAKERS; }
  [[nodiscard]] auto begin() noexcept { return m_gains.begin(); }
  [[nodiscard]] auto end() noexcept { return m_gains.end(); }
  [[nodiscard]] auto begin() const noexcept { return m_gains.begin(); }
  [[nodiscard]] auto end() const noexcept { return m_gains.end(); }

private:
  [[nodiscard]] static std::size_t index(output_patch_t patch) noexcept
  {
    auto const i = patch.get() - 1;
    return static_cast<std::size_t>(
        i < 0 ? 0 : (i >= MAX_NUM_SPEAKERS ? MAX_NUM_SPEAKERS - 1 : i));
  }

  std::array<float, MAX_NUM_SPEAKERS> m_gains{};
};

//==============================================================================
/** The three fields of a SpatGRIS source that the gain computers actually read. */
struct SourceData
{
  std::optional<Position> position{};
  float azimuthSpan{};
  float zenithSpan{};
};

} // namespace Gris
