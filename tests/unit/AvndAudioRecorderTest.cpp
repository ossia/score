// The Audio recorder as the ossia binding drives it: prepared on the main
// thread, then ticked on the audio thread, which hands each tick to a worker
// that writes the file. The worker is run by the test between ticks.

#include <core/document/Document.hpp>

#include <QTemporaryDir>

#include <AvndProcesses/AudioRecorder.hpp>
#include <dr_wav.h>

#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Recorder = avnd_tools::AudioRecorder;

double value(int c, int64_t k)
{
  return 0.25 * (c + 1) + double(k) * 1e-6;
}

struct recorder_harness
{
  static constexpr int max_channels = 4;
  Recorder rec;
  using job = std::pair<std::shared_ptr<Recorder::recorder_thread>, Recorder::worker_message>;
  std::vector<job> jobs;
  std::array<std::vector<double>, max_channels> in_buf;
  std::array<double*, max_channels> in_ptr{};
  int64_t written{};

  explicit recorder_harness(const score::DocumentContext& ctx)
  {
    jobs.reserve(4096);
    rec.ossia_document_context = &ctx;
    rec.worker.request
        = [this](std::shared_ptr<Recorder::recorder_thread> t, Recorder::worker_message m) {
      jobs.emplace_back(std::move(t), std::move(m));
    };
    for(int c = 0; c < max_channels; c++)
    {
      in_buf[c].resize(8192);
      in_ptr[c] = in_buf[c].data();
    }
  }

  //! One tick of `frames` frames: frame k since the start of channel c is
  //! value(c, k).
  void tick(int channels, int frames)
  {
    for(int c = 0; c < channels; c++)
      for(int i = 0; i < frames; i++)
        in_buf[c][i] = value(c, written + i);
    written += frames;
    rec.inputs.audio.samples = in_ptr.data();
    rec.inputs.audio.channels = channels;
    rec(frames);
  }

  //! What the worker thread does with the jobs posted so far.
  void run_worker()
  {
    for(auto& [t, m] : jobs)
      decltype(rec.worker)::work(t, std::move(m));
    jobs.clear();
  }
};
}

TEST_CASE("Audio recorder: prepared again from its tick, it keeps its worker and posts nothing",
          "[avnd][audio][recorder]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    recorder_harness h{doc->context()};
    h.rec.prepare(halp::setup{.input_channels = 2, .output_channels = 0, .frames = 64, .rate = 48000.});
    REQUIRE(h.rec.impl);
    const auto* worker = h.rec.impl.get();
    h.tick(2, 64);
    h.run_worker();

    // The binding prepares again when the input's channel count or the
    // engine buffer change.
    int allocated = 0;
    {
      alloc_counter::scope counter;
      h.rec.prepare(halp::setup{.input_channels = 1, .output_channels = 0, .frames = 128, .rate = 48000.});
      allocated = counter.allocations();
    }
    CHECK(allocated == 0);
    CHECK(h.rec.impl.get() == worker);
    CHECK(h.jobs.empty());
  });
}

TEST_CASE("Audio recorder: a tick allocates nothing, and what it records is in the file",
          "[avnd][audio][recorder]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::string path = dir.filePath("take.wav").toStdString();
    {
      recorder_harness h{doc->context()};
      h.rec.inputs.filename.value = path;
      h.rec.inputs.record.value = true;
      h.rec.inputs.record.prev = true;
      h.rec.prepare(halp::setup{.input_channels = 2, .output_channels = 0, .frames = 64, .rate = 48000.});
      h.tick(2, 64);
      h.run_worker();

      int allocated = 0;
      for(int i = 0; i < 100; i++)
      {
        {
          alloc_counter::scope counter;
          h.tick(2, 64);
          allocated += counter.allocations();
        }
        h.run_worker();
      }
      CHECK(allocated == 0);
      CHECK(h.rec.dropped_blocks == 0);

      // Stops the recording: the file is closed.
      h.rec.inputs.record.value = false;
      h.rec.update();
      h.run_worker();
    }

    drwav wav;
    REQUIRE(drwav_init_file(&wav, path.c_str(), nullptr));
    CHECK(wav.channels == 2);
    CHECK(wav.totalPCMFrameCount == 101 * 64);
    std::vector<float> samples(wav.totalPCMFrameCount * wav.channels);
    drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples.data());
    drwav_uninit(&wav);
    int64_t wrong = 0;
    for(int64_t k = 0; k < int64_t(wav.totalPCMFrameCount); k++)
      for(int c = 0; c < 2; c++)
        wrong += std::abs(samples[2 * k + c] - value(c, k)) > 1e-4;
    CHECK(wrong == 0);
  });
}

TEST_CASE("Audio recorder: with no buffer back from the worker, a tick is dropped, not allocated",
          "[avnd][audio][recorder]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    recorder_harness h{doc->context()};
    h.rec.prepare(halp::setup{.input_channels = 2, .output_channels = 0, .frames = 64, .rate = 48000.});
    h.tick(2, 64);
    h.run_worker();

    // The worker is stalled: the ticks use up the buffers, then are dropped.
    int allocated = 0;
    {
      alloc_counter::scope counter;
      for(int i = 0; i < 1000; i++)
        h.tick(2, 64);
      allocated = counter.allocations();
    }
    CHECK(allocated == 0);
    CHECK(h.rec.dropped_blocks > 0);
    CHECK(int(h.jobs.size()) + h.rec.dropped_blocks == 1000);

    // Back from the worker, the buffers are used again.
    h.run_worker();
    const int dropped = h.rec.dropped_blocks;
    h.tick(2, 64);
    CHECK(h.rec.dropped_blocks == dropped);

    // A tick larger than the buffers is dropped; the worker grows them.
    h.run_worker();
    h.tick(4, 2048);
    CHECK(h.rec.dropped_blocks == dropped + 1);
    h.run_worker();
    CHECK(h.rec.impl->buffer_capacity >= std::size_t(4 * 2048));
  });
}
