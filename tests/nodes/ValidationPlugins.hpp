#pragma once
// The audio plug-ins the CLAP, LV2, VST 2 and VST 3 hosts run in the exec
// sweep: an effect and an instrument of DPF-Plugins, which builds all four
// formats from one source, and two of the LV2 examples. Ubuntu packages them
// as dpf-plugins-{clap,lv2,vst,vst3} and lv2-examples, in /usr/lib.
//
// SCORE_VALIDATION_PLUGINS=<dir> looks for them in <dir>/{clap,lv2,vst,vst3}
// instead. A plug-in that is not there is skipped, and the sweep says so.
//
// The hosts find CLAP, VST 2 and VST 3 plug-ins through their scan, which
// runs in the puppets: the search paths are set to directories that hold
// links to these plug-ins and nothing else, and what the library then lists
// for them is what they are made from. An LV2 plug-in is made from its
// bundle, which the host loads without a scan.
//
// The construction lives in ValidationPlugins.cpp: the headers of the media
// plug-in declare a namespace Media, which ProcessViews.cpp names a struct.

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QString>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace score
{
struct GUIApplicationContext;
}

namespace sweep
{
inline constexpr const char* clap_host = "10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f";
inline constexpr const char* lv2_host = "fd5243ba-70b5-4164-b44a-ecb0dcdc0494";
inline constexpr const char* vst_host = "be8e6bd3-75f2-4102-8895-8a4eb4ea545a";
inline constexpr const char* vst3_host = "4cc30435-3237-4e94-a79e-1c2bd6b724a1";

struct ValidationPlugin
{
  const char* host;
  //! The directory of its format under the root, and its file or bundle there
  const char* format;
  const char* file;
  //! What the library calls it
  const char* name;
  //! The Ubuntu package it comes from
  const char* package;
};

inline constexpr ValidationPlugin validation_plugins[] = {
    {clap_host, "clap", "MaFreeverb.clap", "MaFreeverb", "dpf-plugins-clap"},
    {clap_host, "clap", "Kars.clap", "Kars", "dpf-plugins-clap"},
    {lv2_host, "lv2", "MaFreeverb.lv2", "MaFreeverb", "dpf-plugins-lv2"},
    {lv2_host, "lv2", "Kars.lv2", "Kars", "dpf-plugins-lv2"},
    {lv2_host, "lv2", "eg-amp.lv2", "eg-amp", "lv2-examples"},
    {lv2_host, "lv2", "eg-midigate.lv2", "eg-midigate", "lv2-examples"},
    {vst_host, "vst", "MaFreeverb-vst.so", "MaFreeverb", "dpf-plugins-vst"},
    {vst_host, "vst", "Kars-vst.so", "Kars", "dpf-plugins-vst"},
    {vst3_host, "vst3", "MaFreeverb.vst3", "MaFreeverb", "dpf-plugins-vst3"},
    {vst3_host, "vst3", "Kars.vst3", "Kars", "dpf-plugins-vst3"},
};

inline bool hosts_plugins(const std::string& uuid)
{
  return std::any_of(
      std::begin(validation_plugins), std::end(validation_plugins),
      [&](const ValidationPlugin& p) { return uuid == p.host; });
}

//! The loudest sample the audio outlets of `node` hold
inline double audio_peak(const ossia::graph_node& node)
{
  double peak = 0.;
  for(auto* out : node.root_outputs())
    if(auto* audio = out->target<ossia::audio_port>())
      for(std::size_t c = 0; c < audio->channels(); c++)
        for(double v : audio->channel(c))
          if(std::isfinite(v))
            peak = std::max(peak, std::abs(v));
  return peak;
}

class ValidationPlugins
{
public:
  struct Found
  {
    const ValidationPlugin* plugin{};
    //! What the process is made from; empty when skipped
    QString data;
    std::string skip;
    std::string label() const
    {
      return std::string(plugin->format) + ": " + plugin->name;
    }
  };

  //! The plug-ins of the hosts `wanted` accepts; waits at most `timeout_ms`
  //! for their scan
  ValidationPlugins(
      const score::GUIApplicationContext& ctx,
      const std::function<bool(const std::string&)>& wanted, int timeout_ms = 60000);

  //! What the factory of `uuid` runs: nothing for another factory
  std::vector<const Found*> forHost(const std::string& uuid) const
  {
    std::vector<const Found*> res;
    for(auto& f : m_found)
      if(uuid == f.plugin->host)
        res.push_back(&f);
    return res;
  }

private:
  std::vector<Found> m_found;
};
}
