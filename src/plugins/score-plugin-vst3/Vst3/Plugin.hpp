#pragma once

#define RELEASE 1
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/gui/iplugview.h>

#include <string_view>
#include <QString>

namespace vst3
{
class ApplicationPlugin;
class Model;

inline
QString fromString(const Steinberg::Vst::String128& str) {
#if defined(_WIN32)
  return QString::fromWCharArray(str);
#else
  return QString::fromUtf16(str);
#endif
}
struct Plugin
{
  Plugin() = default;
  Plugin(const Plugin&) = default;
  Plugin(Plugin&&) = default;
  Plugin& operator=(const Plugin&) = default;
  Plugin& operator=(Plugin&&) = default;

  ~Plugin();

  void load(
      Model& model,
      ApplicationPlugin& ctx,
      const std::string& path,
      const std::string& name,
      double sr, int max_bs);
  operator bool() const noexcept { return component && processor; }

  std::string path;
  VST3::Hosting::Module::Ptr mdl;
  Steinberg::Vst::IComponent* component{};
  Steinberg::Vst::IAudioProcessor* processor{};
  Steinberg::Vst::IEditController* controller{};
  Steinberg::IPlugView* view{};

  void loadAudioProcessor(ApplicationPlugin& ctx);
  void loadEditController(
      Model& model,
      ApplicationPlugin& ctx);
  void loadBuses();

  void loadPluginState();

  void start(double sample_rate, int max_bs);
  void stop();

  bool supportsDouble{};
  bool hasUI{};
  int audio_ins  = 0;
  int event_ins  = 0;
  int audio_outs = 0;
  int event_outs = 0;
};

#if __cpp_concepts >= 201907
template<typename T>
concept BusVisitor =
requires (T&& vis) {
    vis.audioIn(Steinberg::Vst::BusInfo{}, int{});
    vis.audioOut(Steinberg::Vst::BusInfo{}, int{});
    vis.eventIn(Steinberg::Vst::BusInfo{}, int{});
    vis.eventOut(Steinberg::Vst::BusInfo{}, int{});
};
void forEachBus(BusVisitor auto&& visitor, Steinberg::Vst::IComponent& component)
#else
template<typename T>
void forEachBus(T&& visitor, Steinberg::Vst::IComponent& component)
#endif
{
  const int audio_ins = component.getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  const int event_ins = component.getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
  const int audio_outs = component.getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  const int event_outs = component.getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);

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
