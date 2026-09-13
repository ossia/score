#pragma once

/**
 * @file Matching a stored MIDI port against the ports a backend is offering
 * now. Shared by the protocols that persist a port in their settings.
 */

#include <libremidi/api.hpp>
#include <libremidi/libremidi.hpp>
#include <libremidi/port_comparison.hpp>

#include <magic_enum/magic_enum.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Protocols::MIDIPorts
{

/**
 * Match a stored port against the live ports of @p api.
 *
 * Returns one of @p live, never the caller's own: a handle means nothing to a
 * backend that did not produce it. It also does not survive a replug, and the
 * MIDI API is a user setting that can change under a saved score -- hence two
 * passes, the first trusting the handle (which is what tells two
 * identically-named ports apart), the second matching on names alone.
 */
template <typename Port>
std::optional<Port>
resolvePort(Port wanted, libremidi::API api, const std::vector<Port>& live)
{
  if(live.empty())
    return std::nullopt;

  const std::span<const Port> ports{live.data(), live.size()};

  // The lookup's heuristics all require the target to name our API.
  if(wanted.api != api)
  {
    wanted.api = api;
    wanted.port = static_cast<libremidi::port_handle>(-1);
  }

  if(auto found = libremidi::optimistic_serialized_port_lookup(wanted, ports);
     !found.empty())
    return *found.front();

  wanted.port = static_cast<libremidi::port_handle>(-1);
  if(auto found = libremidi::optimistic_serialized_port_lookup(wanted, ports);
     !found.empty())
    return *found.front();

  return std::nullopt;
}

inline std::string describePort(const libremidi::port_information& p)
{
  if(!p.display_name.empty())
    return p.display_name;
  if(!p.port_name.empty())
    return p.port_name;
  return "<unnamed>";
}

//! get_api_display_name() is empty for a backend that is not compiled in,
//! which is a case worth naming.
inline std::string apiName(libremidi::API api)
{
  auto name = libremidi::get_api_display_name(api);
  if(name.empty())
    name = magic_enum::enum_name(api);
  return std::string{name};
}

}
