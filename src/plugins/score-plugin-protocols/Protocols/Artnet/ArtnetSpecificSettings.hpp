#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/nullable_variant.hpp>
#include <ossia/detail/variant.hpp>

#include <QString>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <verdigris>

namespace Protocols
{

namespace Artnet
{
struct BaseCapability
{
  QString type;
  QString comment;
  QString effectName;
};

struct SingleCapability : BaseCapability
{
};
enum class MenuClick : int8_t
{
  Start,
  Center,
  End,
  Hidden
};

struct RangeCapability : BaseCapability
{
  std::pair<int, int> range;
  MenuClick menuClick{MenuClick::Start};
};

enum class Diode : int8_t
{
  R,
  G,
  B,
  White,
  WarmWhite,
  ColdWhite,
  Amber,
  UV,
  CCT,
  Empty
};

constexpr int clampedChannels(int64_t diodes, int64_t pixels) noexcept
{
  const int64_t n = diodes * std::max<int64_t>(pixels, 0);
  return n > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : int(n);
}

struct LEDStripLayout
{
  std::vector<Diode> diodes;

  int length{}; // in pixels
  bool reverse{};

  int channels() const noexcept { return clampedChannels(diodes.size(), length); }
};

struct LEDPaneLayout
{
  std::vector<Diode> diodes;

  int width{};
  int height{};

  int channels() const noexcept
  {
    return clampedChannels(diodes.size(), int64_t(width) * height);
  }
};

struct LEDVolumeLayout
{
  std::vector<Diode> diodes;

  int width{};
  int height{};
  int depth{};

  int channels() const noexcept
  {
    return clampedChannels(diodes.size(), int64_t(width) * height * depth);
  }
};

using FixtureCapabilities
    = ossia::variant<SingleCapability, std::vector<RangeCapability>>;

using LEDLayout
    = ossia::nullable_variant<LEDStripLayout, LEDPaneLayout, LEDVolumeLayout>;

struct Channel
{
  QString name;
  FixtureCapabilities capabilities;
  std::vector<QString> fineChannels;
  int defaultValue{};

  //! Number of bytes in which defaultValue and the capability ranges are expressed.
  //! 0 for channels saved before it was recorded.
  int8_t resolution{};
  bool constant{};

  int valueResolution() const noexcept;
};

struct ModeInfo
{
  std::vector<QString> channelNames;
};

struct Fixture
{
  QString fixtureName;
  QString modeName;
  ModeInfo mode;
  std::vector<Channel> controls;
  LEDLayout led;
  int address{};
  int universe{};
};
}

struct ArtnetSpecificSettings
{
  std::vector<Artnet::Fixture> fixtures;

  QString host;
  int rate{44};
  int start_universe{0};
  int universe_count{1};
  int channels_per_universe{512};
  bool multicast{};

  enum
  {
    ArtNet, // Artnet:/Channel-{}
    E131,
    DMXUSBPRO,
    ArtNetV2, // Artnet:/{}
    DMXUSBPRO_Mk2,
    OpenDMX_USB
  } transport{ArtNetV2};
  enum
  {
    Source, // score sends DMX
    Sink    // score receives DMX
  } mode{Source};
};
}

Q_DECLARE_METATYPE(Protocols::ArtnetSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::ArtnetSpecificSettings)
#endif
