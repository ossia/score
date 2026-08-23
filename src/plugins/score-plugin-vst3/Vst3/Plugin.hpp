#pragma once

#define RELEASE 1
#include <ossia/detail/flat_map.hpp>

#include <QString>

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivstunits.h>

#include <string_view>

#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>

namespace vst3
{
class ApplicationPlugin;
class Model;

inline QString fromString(const Steinberg::Vst::String128& str)
{
#if defined(_WIN32)
  return [] <typename T>(const T& str) {
    using char_type = std::decay_t<decltype(str[0])>;
    if constexpr(std::is_same_v<char_type, char16_t>)
      return QString::fromUtf16(str);
    else
      return QString::fromWCharArray(str);
  }(str);
#else
  return QString::fromUtf16(str);
#endif
}

// Keyed by (event bus, MIDI channel 0-15, controller number). The channel is
// part of the key because VST3 lets a plug-in assign a different parameter per
// channel, which is how an MPE-aware plug-in exposes per-voice pitch bend -
// and MPE senders put every note on its own member channel, never channel 0.
using MIDIControls
    = ossia::flat_map<std::tuple<int, int, int>, Steinberg::Vst::ParamID>;
class PlugFrame;
class ComponentHandler;
struct Plugin
{
  Plugin() = default;
  Plugin(const Plugin&) = default;
  Plugin(Plugin&&) = default;
  Plugin& operator=(const Plugin&) = default;
  Plugin& operator=(Plugin&&) = default;

  ~Plugin();

  void load(
      Model& model, ApplicationPlugin& ctx, const std::string& path,
      const VST3::UID& uid, double sr, int max_bs);
  operator bool() const noexcept { return component && processor; }

  std::string path;
  VST3::Hosting::Module::Ptr mdl;
  Steinberg::Vst::IComponent* component{};
  Steinberg::Vst::IAudioProcessor* processor{};
  Steinberg::Vst::IEditController* controller{};
  Steinberg::Vst::IUnitInfo* units{};
  Steinberg::IPlugView* view{};
  PlugFrame* plugFrame{};

  // Connection points of the component and the controller, kept around because
  // VST3 mandates disconnecting them before terminating the plug-in
  Steinberg::Vst::IConnectionPoint* componentCP{};
  Steinberg::Vst::IConnectionPoint* controllerCP{};

  // Given to the controller: it points to the Model, so it has to be unset
  // before the Model dies
  ComponentHandler* componentHandler{};

  void loadAudioProcessor(ApplicationPlugin& ctx);
  void loadEditController(Model& model, ApplicationPlugin& ctx);
  void loadView(Model& model);
  void loadBuses();
  void loadPresets();

  void loadProcessorStateToController();

  void start(double sample_rate, int max_bs);
  void stop();

  bool supports_double{};
  bool ui_available{};
  bool ui_owned{};

  // initialize() succeeded and must be paired with exactly one terminate()
  bool component_initialized{};
  bool controller_initialized{};

  // Single-component effect: the component *is* the edit controller, thus it
  // must only be initialized and terminated once
  bool controller_is_component{};
  int audio_ins = 0;
  int event_ins = 0;
  int audio_outs = 0;
  int event_outs = 0;

  MIDIControls midiControls{};

  Steinberg::Vst::ProgramListInfo programs;
};

#if __cpp_concepts >= 201907
template <typename T>
concept BusVisitor = requires(T&& vis)
{
  vis.audioIn(Steinberg::Vst::BusInfo{}, int{});
  vis.audioOut(Steinberg::Vst::BusInfo{}, int{});
  vis.eventIn(Steinberg::Vst::BusInfo{}, int{});
  vis.eventOut(Steinberg::Vst::BusInfo{}, int{});
};
void forEachBus(BusVisitor auto&& visitor, Steinberg::Vst::IComponent& component)
#else
template <typename T>
void forEachBus(T&& visitor, Steinberg::Vst::IComponent& component)
#endif
{
  const int audio_ins
      = component.getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  const int event_ins
      = component.getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
  const int audio_outs
      = component.getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  const int event_outs
      = component.getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);

  Steinberg::Vst::BusInfo bus;
  for(int i = 0; i < audio_ins; i++)
  {
    component.getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, i, bus);
    visitor.audioIn(bus, i);
  }

  for(int i = 0; i < event_ins; i++)
  {
    component.getBusInfo(Steinberg::Vst::kEvent, Steinberg::Vst::kInput, i, bus);
    visitor.eventIn(bus, i);
  }

  for(int i = 0; i < audio_outs; i++)
  {
    component.getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, i, bus);
    visitor.audioOut(bus, i);
  }

  for(int i = 0; i < event_outs; i++)
  {
    component.getBusInfo(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput, i, bus);
    visitor.eventOut(bus, i);
  }
}
}
