#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <score/tools/std/StringHash.hpp>

#include <QString>

#include <utility>
#include <variant>
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

using FixtureCapabilities
    = std::variant<SingleCapability, std::vector<RangeCapability>>;
struct Channel
{
  QString name;
  FixtureCapabilities capabilities;
  int defaultValue{};
};

struct Fixture
{
  QString fixtureName;
  QString modeName;
  std::vector<Channel> controls;
  int address{};
};

}

struct ArtnetSpecificSettings
{
  std::vector<Artnet::Fixture> fixtures;
  QString host;
  int rate{20};
  int universe{1};
  enum { ArtNet, E131 } transport{ArtNet};
};
}

Q_DECLARE_METATYPE(Protocols::ArtnetSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::ArtnetSpecificSettings)
#endif
