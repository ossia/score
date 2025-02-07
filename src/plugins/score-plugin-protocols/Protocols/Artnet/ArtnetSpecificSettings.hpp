#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/variant.hpp>

#include <QString>

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
struct RangeCapability : BaseCapability
{
  std::pair<int, int> range;
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

struct LEDStripLayout
{
  std::vector<Diode> diodes;

  int length{}; // in pixels
  bool reverse{};

  int channels() const noexcept { return diodes.size() * length; }
};

struct LEDPaneLayout
{
  std::vector<Diode> diodes;

  int width{};
  int height{};

  int channels() const noexcept { return diodes.size() * width * height; }
};

struct LEDVolumeLayout
{
  std::vector<Diode> diodes;

  int width{};
  int height{};
  int depth{};

  int channels() const noexcept { return diodes.size() * width * height * depth; }
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
};
}

struct ArtnetSpecificSettings
{
  std::vector<Artnet::Fixture> fixtures;

  QString host;
  int rate{20};
  int universe{1};
  int channels_per_universe{512};
  bool multicast{};

  enum
  {
    ArtNet, // Artnet:/Channel-{}
    E131,
    DMXUSBPRO,
    ArtNetV2, // Artnet:/{}
    DMXUSBPRO_Mk2,
    OpenDMX_USB,
    ArtNet_MultiUniverse,
    E131_MultiUniverse
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
