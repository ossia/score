// The miniaudio engine on ALSA opens the channels score asks for, and score's
// channel c is the device's channel c, both ways.
//
// ALSA's own plug-ins stand in for the devices, so this runs headless: "file"
// records what is played (and plays a file as the capture), "null" accepts
// any channel count, "multi" over "null" has a fixed count as hardware does.

#include <ossia/audio/miniaudio_protocol.hpp>

#include <catch2/catch_all.hpp>

#if defined(OSSIA_ENABLE_MINIAUDIO) && defined(__linux__)
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace
{
namespace fs = std::filesystem;
using namespace std::chrono_literals;

constexpr int rate = 48000;
constexpr int bs = 256;

//! score's output c plays (c + 1) / 64; its inputs are recorded. ALSA's null
//! plug-in has no clock: the tick paces the device itself.
struct MarkerTick
{
  std::atomic_int* calls;
  std::vector<std::vector<float>>* inputs;
  void operator()(const ossia::audio_tick_state& st) const
  {
    for(int c = 0; c < st.n_out; c++)
      std::fill_n(st.outputs[c], st.frames, float(c + 1) / 64.f);
    if(calls->load() == 10)
    {
      inputs->assign(st.n_in, {});
      for(int c = 0; c < st.n_in; c++)
        (*inputs)[c].assign(st.inputs[c], st.inputs[c] + st.frames);
    }
    (*calls)++;
    std::this_thread::sleep_for(1ms);
  }
};

struct ALSAConfig
{
  fs::path dir;
  ALSAConfig()
  {
    dir = fs::temp_directory_path()
          / ("ossia-alsa-test-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    const auto d = dir.string();
    std::string bindings;
    for(int c = 0; c < 8; c++)
      bindings += "  bindings." + std::to_string(c) + " { slave a; channel "
                  + std::to_string(c) + " }\n";
    // The capture input, channel c at -(c + 1) / 64 in every frame: long
    // enough for the periods that run, unpaced, before the tick is set
    {
      std::vector<float> in(2048 * bs * 8);
      for(std::size_t i = 0; i < in.size(); i++)
        in[i] = -float(i % 8 + 1) / 64.f;
      std::ofstream f{dir / "in8.raw", std::ios::binary};
      f.write(reinterpret_cast<const char*>(in.data()), in.size() * sizeof(float));
    }
    std::ofstream f{dir / "asound.conf"};
    f << "pcm.ossia_multi8 {\n  type multi\n  slaves.a.pcm { type null }\n"
         "  slaves.a.channels 8\n"
      << bindings << "}\n"
      << "pcm.ossia_any { type file slave.pcm { type null } file \"" << d
      << "/any.raw\" format \"raw\" }\n"
      << "pcm.ossia_fixed8 { type file slave.pcm \"ossia_multi8\" file \"" << d
      << "/fixed8.raw\" format \"raw\" }\n"
      << "pcm.ossia_in8 { type file slave.pcm \"ossia_multi8\" file \"/dev/null\" infile \""
      << d << "/in8.raw\" format \"raw\" }\n";
    f.close();
    // Read when the ALSA context is initialized: this file only
    setenv("ALSA_CONFIG_PATH", (dir / "asound.conf").c_str(), 1);
  }
  ~ALSAConfig()
  {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
};

std::shared_ptr<ossia::miniaudio_context> alsa_context()
{
  auto ctx = std::make_unique<ossia::miniaudio_context>();
  ma_backend backends[]{ma_backend_alsa};
  ma_context_config cfg = ma_context_config_init();
  if(ma_context_init(backends, 1, &cfg, &ctx->context) != MA_SUCCESS)
    return {};
  return std::shared_ptr<ossia::miniaudio_context>(
      ctx.release(), [](ossia::miniaudio_context* c) {
    ma_context_uninit(&c->context);
    delete c;
  });
}

ma_device_id alsa_id(const char* name)
{
  ma_device_id id{};
  std::strncpy(id.alsa, name, sizeof(id.alsa) - 1);
  return id;
}

//! Runs until the tick ran some periods, then stops.
void run(ossia::miniaudio_engine& engine, std::atomic_int& calls)
{
  REQUIRE(engine.running());
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while(calls.load() < 20 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  engine.stop();
  REQUIRE(calls.load() >= 20);
}

//! The interleaved frames played: those after the first silent ones.
std::vector<float> played(const fs::path& file, int channels)
{
  std::ifstream f{file, std::ios::binary};
  std::vector<float> all(fs::file_size(file) / sizeof(float));
  f.read(reinterpret_cast<char*>(all.data()), all.size() * sizeof(float));
  std::size_t first = 0;
  while(first < all.size() && all[first] == 0.f)
    first++;
  first -= first % channels;
  return {all.begin() + first, all.end() - all.size() % channels};
}
}

TEST_CASE("ALSA: a device is opened with the channels score asks for", "[audio][alsa]")
{
  ALSAConfig conf;
  auto ctx = alsa_context();
  if(!ctx)
    SKIP("no ALSA");

  // A device that takes any count: asked for none, ALSA opens two
  for(int outputs : {1, 2, 6, 8, 24})
  {
    INFO(outputs << " outputs");
    std::atomic_int calls{};
    std::vector<std::vector<float>> inputs;
    ossia::miniaudio_engine engine{ctx,  "test", ma_device_id{}, alsa_id("ossia_any"),
                                   0,    outputs, rate,          bs};
    REQUIRE(engine.device().playback.format == ma_format_f32);
    CHECK(engine.device().playback.internalChannels == ma_uint32(outputs));
    CHECK(engine.effective_outputs == outputs);
    REQUIRE(engine.device().playback.internalFormat == ma_format_f32);

    engine.set_tick(MarkerTick{&calls, &inputs});
    run(engine, calls);

    const auto out = played(conf.dir / "any.raw", outputs);
    REQUIRE(out.size() >= std::size_t(outputs * bs));
    int wrong = 0;
    for(std::size_t i = 0; i < out.size(); i++)
      wrong += out[i] != float(i % outputs + 1) / 64.f;
    CHECK(wrong == 0);
    fs::remove(conf.dir / "any.raw");
  }
}

TEST_CASE("ALSA: score's channels are the device's, in order", "[audio][alsa]")
{
  ALSAConfig conf;
  auto ctx = alsa_context();
  if(!ctx)
    SKIP("no ALSA");

  // A device of exactly 8 channels, as hardware is
  std::atomic_int calls{};
  std::vector<std::vector<float>> inputs;
  ossia::miniaudio_engine engine{ctx, "test", alsa_id("ossia_in8"), alsa_id("ossia_fixed8"),
                                 8,   8,      rate,                bs};
  CHECK(engine.device().playback.internalChannels == 8);
  CHECK(engine.device().capture.internalChannels == 8);
  CHECK(engine.effective_outputs == 8);
  CHECK(engine.effective_inputs == 8);
  REQUIRE(engine.device().playback.internalFormat == ma_format_f32);
  REQUIRE(engine.device().capture.internalFormat == ma_format_f32);

  engine.set_tick(MarkerTick{&calls, &inputs});
  run(engine, calls);

  const auto out = played(conf.dir / "fixed8.raw", 8);
  REQUIRE(out.size() >= std::size_t(8 * bs));
  int wrong = 0;
  for(std::size_t i = 0; i < out.size(); i++)
    wrong += out[i] != float(i % 8 + 1) / 64.f;
  CHECK(wrong == 0);

  REQUIRE(inputs.size() == 8);
  for(int c = 0; c < 8; c++)
  {
    INFO("input " << c);
    REQUIRE(inputs[c].size() == std::size_t(bs));
    int wrong_in = 0;
    for(float s : inputs[c])
      wrong_in += s != -float(c + 1) / 64.f;
    CHECK(wrong_in == 0);
  }
}

TEST_CASE("ALSA: fewer channels than the device has", "[audio][alsa]")
{
  ALSAConfig conf;
  auto ctx = alsa_context();
  if(!ctx)
    SKIP("no ALSA");

  // As before: miniaudio converts score's two channels to the device's eight
  std::atomic_int calls{};
  std::vector<std::vector<float>> inputs;
  ossia::miniaudio_engine engine{ctx, "test", ma_device_id{}, alsa_id("ossia_fixed8"),
                                 0,   2,      rate,          bs};
  CHECK(engine.device().playback.internalChannels == 8);
  CHECK(engine.effective_outputs == 2);

  engine.set_tick(MarkerTick{&calls, &inputs});
  run(engine, calls);

  const auto out = played(conf.dir / "fixed8.raw", 8);
  REQUIRE(out.size() >= std::size_t(8 * bs));
  // Front left and right get score's left and right
  CHECK(out[0] > 0.f);
  CHECK(out[1] > 0.f);
}
#endif
