#pragma once
// Helpers for the tests of the miniaudio engine on real CoreAudio devices.
// The aggregates built here are private: they only exist in the test process.

#include <ossia/audio/miniaudio_protocol.hpp>

#include <catch2/catch_all.hpp>

#if defined(__APPLE__) && OSSIA_ENABLE_MINIAUDIO
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace coreaudio_test
{
using namespace std::chrono_literals;

inline std::shared_ptr<ossia::miniaudio_context> context()
{
  static auto ctx = [] {
    auto c = std::make_shared<ossia::miniaudio_context>();
    auto cfg = ma_context_config_init();
    ma_backend b[] = {ma_backend_coreaudio};
    REQUIRE(ma_context_init(b, 1, &cfg, &c->context) == MA_SUCCESS);
    return c;
  }();
  return ctx;
}

struct Card
{
  ma_device_id id{};
  std::string name;
  int channels{};
};

inline std::vector<Card> cards(ma_device_type tp)
{
  auto& ctx = context()->context;
  ma_device_info *play, *cap;
  ma_uint32 np, nc;
  REQUIRE(ma_context_get_devices(&ctx, &play, &np, &cap, &nc) == MA_SUCCESS);
  auto infos = tp == ma_device_type_playback ? play : cap;
  auto n = tp == ma_device_type_playback ? np : nc;
  std::vector<Card> out;
  for(ma_uint32 i = 0; i < n; i++)
  {
    ma_device_info full{};
    ma_context_get_device_info(&ctx, tp, &infos[i].id, &full);
    out.push_back(
        {infos[i].id, infos[i].name,
         full.nativeDataFormatCount ? (int)full.nativeDataFormats[0].channels : 0});
  }
  return out;
}

inline std::optional<Card> card(ma_device_type tp, const std::string& name)
{
  for(auto& c : cards(tp))
    if(c.name == name)
      return c;
  return std::nullopt;
}

inline bool has_blackhole()
{
  return card(ma_device_type_playback, "BlackHole 16ch")
         && card(ma_device_type_playback, "BlackHole 64ch");
}

inline std::string uid_of(const std::string& name)
{
  if(auto c = card(ma_device_type_playback, name))
    return c->id.coreaudio;
  if(auto c = card(ma_device_type_capture, name))
    return c->id.coreaudio;
  return {};
}

//! A private aggregate device, destroyed with this object (or before, to
//! simulate the device going away).
struct PrivateAggregate
{
  AudioObjectID id{};
  std::string name;
  explicit PrivateAggregate(std::string nm, std::vector<std::string> sub_uids)
      : name{std::move(nm)}
  {
    auto desc = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    auto cfname = CFStringCreateWithCString(nullptr, name.c_str(), kCFStringEncodingUTF8);
    auto uid = CFStringCreateWithCString(
        nullptr, ("io.ossia.test." + name).c_str(), kCFStringEncodingUTF8);
    CFDictionarySetValue(desc, CFSTR(kAudioAggregateDeviceNameKey), cfname);
    CFDictionarySetValue(desc, CFSTR(kAudioAggregateDeviceUIDKey), uid);
    int one = 1;
    auto yes = CFNumberCreate(nullptr, kCFNumberIntType, &one);
    CFDictionarySetValue(desc, CFSTR(kAudioAggregateDeviceIsPrivateKey), yes);
    auto subs = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);
    for(auto& s : sub_uids)
    {
      auto sub = CFDictionaryCreateMutable(
          nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      auto suid = CFStringCreateWithCString(nullptr, s.c_str(), kCFStringEncodingUTF8);
      CFDictionarySetValue(sub, CFSTR(kAudioSubDeviceUIDKey), suid);
      CFArrayAppendValue(subs, sub);
      CFRelease(suid);
      CFRelease(sub);
    }
    CFDictionarySetValue(desc, CFSTR(kAudioAggregateDeviceSubDeviceListKey), subs);
    REQUIRE(AudioHardwareCreateAggregateDevice(desc, &id) == noErr);
    CFRelease(subs);
    CFRelease(yes);
    CFRelease(uid);
    CFRelease(cfname);
    CFRelease(desc);
    // The HAL publishes the new device asynchronously
    for(int i = 0; i < 100 && !card(ma_device_type_playback, name); i++)
      std::this_thread::sleep_for(20ms);
    REQUIRE(card(ma_device_type_playback, name));
  }
  void destroy()
  {
    if(id)
      AudioHardwareDestroyAggregateDevice(id);
    id = 0;
  }
  ~PrivateAggregate() { destroy(); }
};

//! Counts callbacks; optionally writes a ramp on output 0 and a constant
//! marker on one output, and checks what comes back on the inputs.
struct Probe
{
  static constexpr float ramp_step = 1.f / (1 << 20);
  static constexpr int ramp_len = 4096;
  static constexpr float marker = 1e-4f;

  std::atomic_int callbacks{0};
  std::atomic_int bad_frames{0};
  int expected_frames{};
  int marker_channel{-1};
  int64_t out_pos{};
  // input analysis, audio thread only until the engine stopped
  int64_t in_samples{};
  int64_t ramp_samples{};
  int64_t discontinuities{};
  float prev{};
  bool have_prev{};
  std::vector<double> energy;

  std::vector<int> channels_with_marker() const
  {
    std::vector<int> res;
    for(std::size_t c = 1; c < energy.size(); c++)
      if(in_samples > 0 && energy[c] / in_samples > marker * 0.05)
        res.push_back(c);
    return res;
  }
};

struct ProbeTick
{
  Probe* p;
  void operator()(const ossia::audio_tick_state& st) const
  {
    p->callbacks++;
    if((int)st.frames != p->expected_frames)
      p->bad_frames++;
    if(p->energy.size() < st.n_in)
      return;
    for(int c = 0; c < st.n_in; c++)
      for(std::size_t f = 0; f < st.frames; f++)
        p->energy[c] += std::abs(st.inputs[c][f]);
    for(std::size_t f = 0; st.n_in > 0 && f < st.frames; f++)
    {
      const float v = st.inputs[0][f];
      p->in_samples++;
      if(v != 0.f)
        p->ramp_samples++;
      if(p->have_prev)
      {
        float e = p->prev + Probe::ramp_step;
        if(e >= Probe::ramp_len * Probe::ramp_step)
          e = 0.f;
        if(v != e)
          p->discontinuities++;
      }
      p->prev = v;
      p->have_prev = p->ramp_samples > 0;
    }
    for(std::size_t f = 0; f < st.frames; f++)
    {
      if(st.n_out > 0)
        st.outputs[0][f] = float((p->out_pos + f) % Probe::ramp_len) * Probe::ramp_step;
      if(p->marker_channel > 0 && p->marker_channel < (int)st.n_out)
        st.outputs[p->marker_channel][f] = Probe::marker;
    }
    p->out_pos += st.frames;
  }
};

//! The HAL object of a device, by name
inline AudioObjectID hal_device(const std::string& name)
{
  AudioObjectPropertyAddress a{
      kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size);
  std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
  AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data());
  for(auto id : ids)
  {
    CFStringRef s{};
    UInt32 sz = sizeof(s);
    AudioObjectPropertyAddress n{
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    char buf[256]{};
    if(AudioObjectGetPropertyData(id, &n, 0, nullptr, &sz, &s) == noErr && s)
    {
      CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
      CFRelease(s);
    }
    if(name == buf)
      return id;
  }
  return 0;
}

inline double nominal_rate(AudioObjectID dev)
{
  Float64 r{};
  UInt32 sz = sizeof(r);
  AudioObjectPropertyAddress a{
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  AudioObjectGetPropertyData(dev, &a, 0, nullptr, &sz, &r);
  return r;
}

inline bool set_nominal_rate(AudioObjectID dev, double rate)
{
  Float64 r = rate;
  AudioObjectPropertyAddress a{
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  if(AudioObjectSetPropertyData(dev, &a, 0, nullptr, sizeof(r), &r) != noErr)
    return false;
  for(int i = 0; i < 100 && nominal_rate(dev) != rate; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  return nominal_rate(dev) == rate;
}

//! Puts a device's nominal rate back as it was, whatever the test did to it:
//! the rate is a system-wide setting.
struct NominalRateGuard
{
  AudioObjectID dev{};
  double saved{};
  explicit NominalRateGuard(const std::string& name)
      : dev{hal_device(name)}
      , saved{nominal_rate(dev)}
  {
    REQUIRE(dev);
  }
  ~NominalRateGuard()
  {
    if(nominal_rate(dev) != saved)
      set_nominal_rate(dev, saved);
  }
};

//! Every device's nominal rate, put back when done: score switches the rate
//! of the devices it opens (the system's default ones too, in a fresh app)
//! when nothing else uses them. Only those that changed are touched.
struct AllNominalRatesGuard
{
  std::vector<std::pair<AudioObjectID, double>> saved;
  AllNominalRatesGuard()
  {
    const AudioObjectPropertyAddress addr{
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 size{};
    if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size)
       != noErr)
      return;
    std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, ids.data())
       != noErr)
      return;
    ids.resize(size / sizeof(AudioObjectID));
    for(auto id : ids)
      if(const double r = nominal_rate(id); r > 0)
        saved.emplace_back(id, r);
  }
  ~AllNominalRatesGuard()
  {
    for(auto [id, rate] : saved)
      if(nominal_rate(id) != rate)
        set_nominal_rate(id, rate);
  }
};

//! Another client of a device: a raw HAL IO proc, which makes the device
//! "running somewhere" as another program using it would.
struct OtherClient
{
  AudioObjectID dev{};
  AudioDeviceIOProcID proc{};
  explicit OtherClient(const std::string& name)
      : dev{hal_device(name)}
  {
    REQUIRE(dev);
    REQUIRE(
        AudioDeviceCreateIOProcIDWithBlock(
            &proc, dev, nullptr,
            ^(const AudioTimeStamp*, const AudioBufferList*, const AudioTimeStamp*,
              AudioBufferList*, const AudioTimeStamp*) {})
        == noErr);
    REQUIRE(AudioDeviceStart(dev, proc) == noErr);
  }
  ~OtherClient()
  {
    AudioDeviceStop(dev, proc);
    AudioDeviceDestroyIOProcID(dev, proc);
  }
};

inline ma_device_id no_device()
{
  return ma_device_id{};
}
}

#endif
