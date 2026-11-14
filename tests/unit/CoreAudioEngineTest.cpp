// The miniaudio engine against the real CoreAudio devices of the machine.
//
// Hardware-gated: needs macOS with BlackHole (16ch and 64ch) installed, which
// gives devices whose output can be read back sample-exactly; skipped
// elsewhere. The aggregates these tests build are private: they only exist in
// this process, the machine's audio setup is left untouched.

#include "CoreAudioTestUtils.hpp"

#if defined(__APPLE__) && OSSIA_ENABLE_MINIAUDIO
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace coreaudio_test;
using namespace std::chrono_literals;

namespace
{
// miniaudio calls CoreAudio through the function pointers of its context.
// These refuse to bind an AudioUnit to its device - as the HAL does when the
// device went away between enumeration and opening - and record every use of
// an AudioUnit after it was disposed of.
struct AudioUnitSpy
{
  ma_proc set{}, get{}, init{}, dispose{}, add_listener{};
  std::vector<void*> disposed;
  int uses_after_dispose{};
  int double_dispose{};
  int refused{};
  bool was_disposed(void* u) const
  {
    return std::find(disposed.begin(), disposed.end(), u) != disposed.end();
  }
} g_spy;

using SetProp = OSStatus (*)(
    AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, const void*, UInt32);
using GetProp = OSStatus (*)(
    AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, void*, UInt32*);
using InitUnit = OSStatus (*)(AudioUnit);
using Dispose = OSStatus (*)(AudioComponentInstance);
using AddListener
    = OSStatus (*)(AudioUnit, AudioUnitPropertyID, AudioUnitPropertyListenerProc, void*);

OSStatus spySet(
    AudioUnit u, AudioUnitPropertyID id, AudioUnitScope s, AudioUnitElement e,
    const void* d, UInt32 sz)
{
  if(g_spy.was_disposed(u))
  {
    g_spy.uses_after_dispose++;
    return kAudioUnitErr_Uninitialized;
  }
  if(id == kAudioOutputUnitProperty_CurrentDevice)
  {
    g_spy.refused++;
    return kAudioHardwareBadDeviceError;
  }
  return ((SetProp)g_spy.set)(u, id, s, e, d, sz);
}
OSStatus spyGet(
    AudioUnit u, AudioUnitPropertyID id, AudioUnitScope s, AudioUnitElement e, void* d,
    UInt32* sz)
{
  if(g_spy.was_disposed(u))
  {
    g_spy.uses_after_dispose++;
    return kAudioUnitErr_Uninitialized;
  }
  return ((GetProp)g_spy.get)(u, id, s, e, d, sz);
}
OSStatus spyInit(AudioUnit u)
{
  if(g_spy.was_disposed(u))
  {
    g_spy.uses_after_dispose++;
    return kAudioUnitErr_Uninitialized;
  }
  return ((InitUnit)g_spy.init)(u);
}
OSStatus spyAddListener(
    AudioUnit u, AudioUnitPropertyID id, AudioUnitPropertyListenerProc p, void* d)
{
  if(g_spy.was_disposed(u))
  {
    g_spy.uses_after_dispose++;
    return kAudioUnitErr_Uninitialized;
  }
  return ((AddListener)g_spy.add_listener)(u, id, p, d);
}
OSStatus spyDispose(AudioComponentInstance u)
{
  if(g_spy.was_disposed(u))
  {
    // Not forwarded: a second dispose of the same instance is the bug itself
    g_spy.double_dispose++;
    return noErr;
  }
  g_spy.disposed.push_back(u);
  return ((Dispose)g_spy.dispose)(u);
}
}

TEST_CASE("A device that cannot be bound fails to open, cleanly", "[coreaudio][hardware]")
{
  // miniaudio disposed of the AudioUnit, then reported success: the device
  // initialization went on with the freed unit.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  auto dev = card(ma_device_type_playback, "BlackHole 16ch");

  auto ctx = std::make_shared<ossia::miniaudio_context>();
  auto cfg = ma_context_config_init();
  ma_backend b[] = {ma_backend_coreaudio};
  REQUIRE(ma_context_init(b, 1, &cfg, &ctx->context) == MA_SUCCESS);
  auto& ca = ctx->context.coreaudio;
  g_spy = {};
  g_spy.set = std::exchange(ca.AudioUnitSetProperty, (ma_proc)&spySet);
  g_spy.get = std::exchange(ca.AudioUnitGetProperty, (ma_proc)&spyGet);
  g_spy.init = std::exchange(ca.AudioUnitInitialize, (ma_proc)&spyInit);
  g_spy.add_listener = std::exchange(ca.AudioUnitAddPropertyListener, (ma_proc)&spyAddListener);
  g_spy.dispose = std::exchange(ca.AudioComponentInstanceDispose, (ma_proc)&spyDispose);

  for(auto [ins, outs] : {std::pair{0, 16}, std::pair{16, 0}, std::pair{16, 16}})
  {
    INFO("inputs " << ins << " outputs " << outs);
    g_spy.uses_after_dispose = 0;
    g_spy.double_dispose = 0;
    g_spy.refused = 0;
    CHECK_THROWS_AS(
        std::make_unique<ossia::miniaudio_engine>(
            ctx, "test", ins ? dev->id : no_device(), outs ? dev->id : no_device(), ins,
            outs, 48000, 256),
        std::runtime_error);
    CHECK(g_spy.refused > 0);
    CHECK(g_spy.uses_after_dispose == 0);
    CHECK(g_spy.double_dispose == 0);
  }

  ca.AudioUnitSetProperty = g_spy.set;
  ca.AudioUnitGetProperty = g_spy.get;
  ca.AudioUnitInitialize = g_spy.init;
  ca.AudioUnitAddPropertyListener = g_spy.add_listener;
  ca.AudioComponentInstanceDispose = g_spy.dispose;
  ma_context_uninit(&ctx->context);
}

TEST_CASE("A device that goes away is reported as failed", "[coreaudio][hardware]")
{
  // Unplugging a card (Thunderbolt, USB), a virtual device whose driver
  // restarts, an aggregate losing its sub-device: CoreAudio moves the stream
  // to the default output, then stops calling back, and the engine claimed to
  // be running fine.
  if(!has_blackhole())
    SKIP("BlackHole not installed");

  for(int bs : {256, 2048})
  {
    INFO("buffer size " << bs);
    PrivateAggregate agg{"ossia-test-lost", {uid_of("BlackHole 16ch")}};
    auto dev = card(ma_device_type_playback, agg.name);
    REQUIRE(dev);

    ossia::miniaudio_engine engine{context(), "test", dev->id, dev->id, 16, 16, 48000, bs};
    Probe probe;
    probe.expected_frames = bs;
    probe.energy.resize(16);
    REQUIRE(engine.set_tick(ProbeTick{&probe}));
    std::this_thread::sleep_for(500ms);
    REQUIRE(probe.callbacks > 0);
    REQUIRE(!engine.failed());

    agg.destroy();

    const auto t0 = std::chrono::steady_clock::now();
    while(!engine.failed() && std::chrono::steady_clock::now() - t0 < 2s)
      std::this_thread::sleep_for(10ms);
    CHECK(engine.failed());
    CHECK(std::chrono::steady_clock::now() - t0 < 1s);

    // Nothing of score's plays wherever CoreAudio moved the stream to
    const int after = probe.callbacks;
    std::this_thread::sleep_for(300ms);
    CHECK(probe.callbacks == after);

    // Play / stop install ticks: that must not hang on a dead device
    const auto t1 = std::chrono::steady_clock::now();
    engine.set_tick({});
    CHECK(std::chrono::steady_clock::now() - t1 < 100ms);
  }
}

namespace
{
struct Loopback
{
  int callbacks{};
  int64_t ramp_samples{};
  int64_t discontinuities{};
  std::vector<int> marker_on;
  int playback_channels{}, capture_channels{};
  bool playback_passthrough{}, capture_passthrough{};
  double measured_rate{};
};

//! Score asks for `chans` in and out of `device`; output 0 carries a ramp,
//! output `marker` a constant: what comes back on the inputs?
Loopback loopback(const std::string& device, int chans, int marker, int bs = 512,
                  std::chrono::milliseconds dur = 1500ms)
{
  auto dev = card(ma_device_type_playback, device);
  REQUIRE(dev);
  ossia::miniaudio_engine engine{context(), "test", dev->id, dev->id, chans, chans, 48000, bs};
  Probe probe;
  probe.expected_frames = bs;
  probe.marker_channel = marker;
  probe.energy.resize(chans);

  std::vector<double> times;
  times.reserve(1 << 16);
  struct Timed
  {
    ProbeTick tick;
    std::vector<double>* times;
    void operator()(const ossia::audio_tick_state& st) const
    {
      if(times->size() < times->capacity())
        times->push_back(std::chrono::duration<double>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count());
      tick(st);
    }
  };
  REQUIRE(engine.set_tick(Timed{ProbeTick{&probe}, &times}));
  std::this_thread::sleep_for(dur);
  REQUIRE(engine.set_tick({}));

  Loopback res;
  res.callbacks = probe.callbacks;
  res.ramp_samples = probe.ramp_samples;
  res.discontinuities = probe.discontinuities;
  res.marker_on = probe.channels_with_marker();
  // What miniaudio opened the device with
  auto& d = engine.device();
  res.playback_channels = d.playback.internalChannels;
  res.capture_channels = d.capture.internalChannels;
  res.playback_passthrough = d.playback.converter.isPassthrough;
  res.capture_passthrough = d.capture.converter.isPassthrough;
  if(times.size() > 20)
    res.measured_rate = (times.size() - 11) * double(bs) / (times.back() - times[10]);
  return res;
}
}

TEST_CASE("Every channel of a multi-stream device is reachable", "[coreaudio][hardware]")
{
  // An aggregate has one stream per sub-device: miniaudio opened only the
  // first one (here 16 of 80 channels), and converted the 80 channels score
  // asked for into those.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  PrivateAggregate agg{"ossia-test-80", {uid_of("BlackHole 16ch"), uid_of("BlackHole 64ch")}};
  REQUIRE(card(ma_device_type_playback, agg.name)->channels == 80);

  for(int marker : {5, 15, 16, 40, 79})
  {
    INFO("marker on " << marker);
    auto res = loopback(agg.name, 80, marker);
    CHECK(res.playback_channels == 80);
    CHECK(res.capture_channels == 80);
    CHECK(res.ramp_samples > 0);
    CHECK(res.discontinuities == 0);
    CHECK(res.marker_on == std::vector<int>{marker});
  }
}

TEST_CASE("Channels map one to one, whatever count score asks for", "[coreaudio][hardware]")
{
  // Fewer or more channels than the device has: miniaudio's converter mixed
  // every channel from the 33rd on into every other one.
  if(!has_blackhole())
    SKIP("BlackHole not installed");

  SECTION("fewer than the device")
  {
    for(int marker : {5, 40, 47})
    {
      INFO("marker on " << marker);
      auto res = loopback("BlackHole 64ch", 48, marker);
      CHECK(res.discontinuities == 0);
      CHECK(res.marker_on == std::vector<int>{marker});
      CHECK(res.playback_passthrough);
      CHECK(res.capture_passthrough);
    }
  }
  SECTION("more than the device")
  {
    // Outputs past the device's last channel go nowhere; inputs past it are silent
    auto res = loopback("BlackHole 16ch", 40, 12);
    CHECK(res.discontinuities == 0);
    CHECK(res.marker_on == std::vector<int>{12});
    res = loopback("BlackHole 16ch", 40, 35);
    CHECK(res.marker_on.empty());
    CHECK(res.playback_passthrough);
    CHECK(res.capture_passthrough);
  }
}

TEST_CASE("Hundreds of channels keep up in real time", "[coreaudio][hardware][perf]")
{
  // 384 channels over two streams, like the C256 + BlackHole aggregates: the
  // channel conversion could only deliver a fraction of real time.
  if(!has_blackhole() || !card(ma_device_type_playback, "BlackHole 256ch")
     || !card(ma_device_type_playback, "BlackHole 128ch"))
    SKIP("BlackHole 128ch / 256ch not installed");
  PrivateAggregate agg{
      "ossia-test-384", {uid_of("BlackHole 256ch"), uid_of("BlackHole 128ch")}};

  for(int bs : {32, 128, 512})
  {
    INFO("buffer size " << bs);
    auto res = loopback(agg.name, 384, 300, bs, 2s);
    CHECK(res.playback_channels == 384);
    CHECK(res.measured_rate == Catch::Approx(48000).epsilon(0.01));
    CHECK(res.discontinuities == 0);
    CHECK(res.marker_on == std::vector<int>{300});
  }
}

TEST_CASE("The engine runs at the rate it reports", "[coreaudio][hardware][rate]")
{
  // The device was left at its rate and miniaudio resampled to the one score
  // asked for, which score then believed it ran at.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  NominalRateGuard bh16{"BlackHole 16ch"};
  REQUIRE(set_nominal_rate(bh16.dev, 48000));
  auto dev = card(ma_device_type_playback, "BlackHole 16ch");

  auto open = [&](int rate) {
    return std::make_unique<ossia::miniaudio_engine>(
        context(), "test", dev->id, dev->id, 16, 16, rate, 512);
  };

  SECTION("a rate the device supports, nobody else using it: the device switches")
  {
    auto e = open(96000);
    CHECK(e->effective_sample_rate == 96000);
    CHECK(nominal_rate(bh16.dev) == 96000);
    CHECK(e->device().playback.internalSampleRate == 96000);
    CHECK(e->device().playback.converter.isPassthrough);
    CHECK(e->device().capture.converter.isPassthrough);
  }
  SECTION("a rate the device does not support: the device's own")
  {
    auto e = open(22050);
    CHECK(e->effective_sample_rate == 48000);
    CHECK(nominal_rate(bh16.dev) == 48000);
    CHECK(e->device().playback.converter.isPassthrough);
    CHECK(e->device().capture.converter.isPassthrough);
  }
  SECTION("another program uses the device: its rate is left alone")
  {
    OtherClient reaper{"BlackHole 16ch"};
    auto e = open(96000);
    CHECK(e->effective_sample_rate == 48000);
    CHECK(nominal_rate(bh16.dev) == 48000);
    CHECK(e->device().playback.converter.isPassthrough);
  }
}

TEST_CASE("A rate changed under a running engine is reported", "[coreaudio][hardware][rate]")
{
  // Audio MIDI Setup, or another program, switching the device's rate: score
  // only restarts at the new rate if the engine tells it.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  NominalRateGuard bh16{"BlackHole 16ch"};
  REQUIRE(set_nominal_rate(bh16.dev, 48000));
  auto dev = card(ma_device_type_playback, "BlackHole 16ch");

  ossia::miniaudio_engine e{context(), "test", dev->id, dev->id, 16, 16, 48000, 256};
  Probe probe;
  probe.expected_frames = 256;
  probe.energy.resize(16);
  REQUIRE(e.set_tick(ProbeTick{&probe}));
  std::this_thread::sleep_for(300ms);
  CHECK(e.observed_sample_rate.load() == 48000);

  REQUIRE(set_nominal_rate(bh16.dev, 96000));
  const auto t0 = std::chrono::steady_clock::now();
  while(e.observed_sample_rate.load() != 96000
        && std::chrono::steady_clock::now() - t0 < 2s)
    std::this_thread::sleep_for(10ms);
  CHECK(e.observed_sample_rate.load() == 96000);
  CHECK(!e.failed());
  e.set_tick({});
}

TEST_CASE("A buffer size above the device's maximum is the maximum", "[coreaudio][hardware][buffer]")
{
  // The device clamps its buffer, and the callbacks, still of the size asked
  // for, came at uneven intervals: three in four periods, each with a whole
  // period's deadline.
  if(!has_blackhole())
    SKIP("BlackHole not installed");

  struct Case
  {
    std::string device;
    int channels, asked, expected;
  };
  std::vector<Case> cases{{"BlackHole 16ch", 16, 8192, 4096}};
  // The C256's maximum is 1536, below the 2048 score offers
  if(card(ma_device_type_playback, "C256 (Studio de son)"))
    cases.push_back({"C256 (Studio de son)", 256, 2048, 1536});

  for(auto& c : cases)
  {
    INFO(c.device << " asked " << c.asked);
    auto dev = card(ma_device_type_playback, c.device);
    // Silent: the C256 is a real card
    ossia::miniaudio_engine e{
        context(), "test", no_device(), dev->id, 0, c.channels, 48000, c.asked};
    CHECK(e.effective_buffer_size == c.expected);
    CHECK((int)e.device().playback.internalPeriodSizeInFrames == c.expected);

    std::vector<double> times;
    times.reserve(4096);
    std::atomic_int bad_frames{0};
    struct Timed
    {
      std::vector<double>* times;
      std::atomic_int* bad;
      int expected;
      void operator()(const ossia::audio_tick_state& st) const
      {
        if((int)st.frames != expected)
          (*bad)++;
        if(times->size() < times->capacity())
          times->push_back(std::chrono::duration<double>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count());
      }
    };
    REQUIRE(e.set_tick(Timed{&times, &bad_frames, c.expected}));
    std::this_thread::sleep_for(2s);
    REQUIRE(e.set_tick({}));

    CHECK(bad_frames == 0);
    REQUIRE(times.size() > 10);
    const double period = double(c.expected) / 48000;
    int late = 0;
    for(std::size_t i = 4; i < times.size(); i++)
      if(times[i] - times[i - 1] > 1.5 * period)
        late++;
    CHECK(late == 0);
  }
}

TEST_CASE("Only the system's default devices are listed as default", "[coreaudio][hardware][devices]")
{
  // A device that is the default output was also listed as the default input
  // (whatever the default input was), and score then picked it as such.
  auto& ctx = context()->context;
  ma_device_info *play, *cap;
  ma_uint32 np, nc;
  REQUIRE(ma_context_get_devices(&ctx, &play, &np, &cap, &nc) == MA_SUCCESS);

  auto hal_default = [](AudioObjectPropertySelector sel) {
    AudioObjectID dev{};
    UInt32 size = sizeof(dev);
    AudioObjectPropertyAddress a{
        sel, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, &dev);
    CFStringRef uid{};
    size = sizeof(uid);
    AudioObjectPropertyAddress u{
        kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    char buf[256]{};
    if(AudioObjectGetPropertyData(dev, &u, 0, nullptr, &size, &uid) == noErr && uid)
    {
      CFStringGetCString(uid, buf, sizeof(buf), kCFStringEncodingUTF8);
      CFRelease(uid);
    }
    return std::string(buf);
  };

  auto defaults = [](ma_device_info* infos, ma_uint32 n) {
    std::vector<std::string> res;
    for(ma_uint32 i = 0; i < n; i++)
      if(infos[i].isDefault)
        res.push_back(infos[i].id.coreaudio);
    return res;
  };

  const auto def_out = hal_default(kAudioHardwarePropertyDefaultOutputDevice);
  const auto def_in = hal_default(kAudioHardwarePropertyDefaultInputDevice);
  if(def_out.empty() || def_in.empty())
    SKIP("no default device");
  CHECK(defaults(play, np) == std::vector<std::string>{def_out});
  CHECK(defaults(cap, nc) == std::vector<std::string>{def_in});
}

namespace
{
// The capture side of a two-device duplex losing a period now and then: what
// a capture device on its own clock, running slower than the playback one,
// does. miniaudio's input callback gets its data through AudioUnitRender.
ma_proc g_real_AudioUnitRender{};
std::atomic_int g_renders{0};
std::atomic_int g_dropped{0};
OSStatus dropSomeRenders(
    AudioUnit u, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts, UInt32 bus,
    UInt32 frames, AudioBufferList* data)
{
  if(++g_renders % 50 == 0)
  {
    g_dropped++;
    return kAudioUnitErr_CannotDoInCurrentContext;
  }
  using fn = OSStatus (*)(
      AudioUnit, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32,
      AudioBufferList*);
  return ((fn)g_real_AudioUnitRender)(u, flags, ts, bus, frames, data);
}

//! Writes a sample-exact ramp on output 0, counts the frames it is asked for.
struct RampTick
{
  int64_t* pos;
  std::atomic<int64_t>* frames;
  void operator()(const ossia::audio_tick_state& st) const
  {
    for(std::size_t f = 0; f < st.frames; f++)
      st.outputs[0][f] = float((*pos + f) % Probe::ramp_len) * Probe::ramp_step;
    *pos += st.frames;
    *frames += st.frames;
  }
};
}

TEST_CASE("Two devices in duplex: an input shortfall does not reach the output", "[coreaudio][hardware][duplex]")
{
  // When the capture device had not delivered yet, the output buffer was left
  // partly unwritten - whatever was in it played - and the execution did not
  // run for those frames, so it fell behind the audio clock.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  auto capture = card(ma_device_type_capture, "BlackHole 64ch");
  auto playback = card(ma_device_type_playback, "BlackHole 16ch");

  auto ctx = std::make_shared<ossia::miniaudio_context>();
  auto cfg = ma_context_config_init();
  ma_backend b[] = {ma_backend_coreaudio};
  REQUIRE(ma_context_init(b, 1, &cfg, &ctx->context) == MA_SUCCESS);
  g_real_AudioUnitRender
      = std::exchange(ctx->context.coreaudio.AudioUnitRender, (ma_proc)&dropSomeRenders);
  g_renders = 0;
  g_dropped = 0;

  // What reaches BlackHole 16ch's output, read back by another engine
  ossia::miniaudio_engine observer{context(), "observer", playback->id, no_device(), 16, 0, 48000, 256};
  Probe seen;
  seen.expected_frames = 256;
  seen.energy.resize(16);
  REQUIRE(observer.set_tick(ProbeTick{&seen}));

  int64_t pos = 0;
  std::atomic<int64_t> ticked{0};
  {
    ossia::miniaudio_engine duplex{ctx, "duplex", capture->id, playback->id, 64, 16, 48000, 256};
    REQUIRE(duplex.set_tick(RampTick{&pos, &ticked}));
    const auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(3s);
    const double elapsed
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    // Stop reading before the ramp stops, which would read as a break
    REQUIRE(observer.set_tick({}));
    REQUIRE(duplex.set_tick({}));
    REQUIRE(g_dropped > 5);

    // The execution keeps up with the audio clock
    CHECK(ticked.load() / elapsed == Catch::Approx(48000).epsilon(0.02));
  }
  // The output is what the execution wrote, sample for sample
  CHECK(seen.ramp_samples > 48000);
  CHECK(seen.discontinuities == 0);

  ctx->context.coreaudio.AudioUnitRender = g_real_AudioUnitRender;
  ma_context_uninit(&ctx->context);
}

TEST_CASE("Two devices in duplex run on one clock", "[coreaudio][hardware][duplex]")
{
  // An input and an output on different cards have different clocks: they
  // drift apart, and the input then skips or repeats. CoreAudio's answer is
  // an aggregate device, the input sub-device's drift compensated against the
  // output's clock.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  auto capture = card(ma_device_type_capture, "BlackHole 64ch");
  auto playback = card(ma_device_type_playback, "BlackHole 16ch");
  auto devices_before = cards(ma_device_type_playback).size();

  {
    // Writes a marker on BlackHole 64ch input 37, through its loopback
    ossia::miniaudio_engine writer{context(), "writer", no_device(), capture->id, 0, 64, 48000, 256};
    Probe w;
    w.expected_frames = 256;
    w.marker_channel = 37;
    REQUIRE(writer.set_tick(ProbeTick{&w}));

    ossia::miniaudio_engine duplex{context(), "duplex", capture->id, playback->id, 64, 16, 48000, 256};
    CHECK(duplex.effective_inputs == 64);
    CHECK(duplex.effective_outputs == 16);

    // One device for both directions
    const auto& d = duplex.device();
    CHECK(d.coreaudio.deviceObjectIDPlayback == d.coreaudio.deviceObjectIDCapture);

    // ... whose input side follows the output's clock
    AudioObjectID agg = d.coreaudio.deviceObjectIDCapture;
    UInt32 size{};
    AudioObjectPropertyAddress subs{
        kAudioAggregateDevicePropertyActiveSubDeviceList, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    REQUIRE(AudioObjectGetPropertyDataSize(agg, &subs, 0, nullptr, &size) == noErr);
    std::vector<AudioObjectID> sub(size / sizeof(AudioObjectID));
    REQUIRE(AudioObjectGetPropertyData(agg, &subs, 0, nullptr, &size, sub.data()) == noErr);
    REQUIRE(sub.size() == 2);
    CHECK(sub[0] == hal_device("BlackHole 16ch"));
    CHECK(sub[1] == hal_device("BlackHole 64ch"));
    // The drift compensation is a property of the aggregate's sub-device
    // objects, not of the devices themselves
    auto drift_of = [&](const std::string& device_uid) {
      AudioClassID cls = kAudioSubDeviceClassID;
      AudioObjectPropertyAddress owned{
          kAudioObjectPropertyOwnedObjects, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      UInt32 sz{};
      AudioObjectGetPropertyDataSize(agg, &owned, sizeof(cls), &cls, &sz);
      std::vector<AudioObjectID> objs(sz / sizeof(AudioObjectID));
      AudioObjectGetPropertyData(agg, &owned, sizeof(cls), &cls, &sz, objs.data());
      for(auto o : objs)
      {
        CFStringRef u{};
        UInt32 usz = sizeof(u);
        AudioObjectPropertyAddress ua{
            kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain};
        char buf[256]{};
        if(AudioObjectGetPropertyData(o, &ua, 0, nullptr, &usz, &u) == noErr && u)
        {
          CFStringGetCString(u, buf, sizeof(buf), kCFStringEncodingUTF8);
          CFRelease(u);
        }
        if(device_uid == buf)
        {
          UInt32 drift{};
          UInt32 dsz = sizeof(drift);
          AudioObjectPropertyAddress da{
              kAudioSubDevicePropertyDriftCompensation, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain};
          AudioObjectGetPropertyData(o, &da, 0, nullptr, &dsz, &drift);
          return (int)drift;
        }
      }
      return -1;
    };
    CHECK(drift_of(capture->id.coreaudio) == 1);

    // score's input k is the input device's input k
    Probe in;
    in.expected_frames = 256;
    in.energy.resize(64);
    REQUIRE(duplex.set_tick(ProbeTick{&in}));
    std::this_thread::sleep_for(1s);
    REQUIRE(duplex.set_tick({}));
    REQUIRE(writer.set_tick({}));
    CHECK(in.channels_with_marker() == std::vector<int>{37});
  }

  // The aggregate does not outlive the engine
  CHECK(cards(ma_device_type_playback).size() == devices_before);
}

#endif
