#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <QStringList>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Protocols::OFL
{
struct FixtureMode
{
  QString name;

  //! One entry per DMX slot of the mode, empty for a slot without function.
  std::vector<QString> allChannels;

  std::vector<Artnet::Channel> channels;

  QString content() const noexcept;
};

struct FixtureDefinition
{
  QString name;
  QStringList categories;
  std::vector<FixtureMode> modes;
};

//! Parses an Open Fixture Library fixture definition.
//! Returns nullopt for invalid documents and for redirects.
std::optional<FixtureDefinition> parseFixture(std::string_view json);

//! Also follows a redirect to the same fixture sold under another brand, reading the
//! target fixture through readFixture ("manufacturer/fixture" key), under the
//! redirect's own name.
std::optional<FixtureDefinition> parseFixture(
    std::string_view json,
    const std::function<std::optional<std::string>(const QString&)>& readFixture);

//! DMX value scaling between resolutions (in bytes), as OFL's scale-dmx-values.
int64_t scaleValue(int64_t value, int from, int to) noexcept;
std::pair<int64_t, int64_t>
scaleRange(int64_t start, int64_t end, int from, int to) noexcept;

//! The value a capability sets when chosen as a preset, or nullopt when it is hidden.
std::optional<int64_t>
menuClickValue(const Artnet::RangeCapability& capability, int from, int to) noexcept;
}
#endif
