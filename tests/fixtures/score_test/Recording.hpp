#pragma once

// What the recording tests share: sounds with known samples to record, and
// reading back what was written.

#include <score_test/Events.hpp>

#include <QString>

#include <catch2/catch_test_macros.hpp>
#include <dr_wav.h>

#include <vector>

namespace score::test
{

//! Runs the event loop for `ms`, the engine playing meanwhile
inline void spin(int ms)
{
  run_events_for(ms);
}

//! Runs the event loop until `f` holds, `ms` at most; whether it held. Unlike
//! wait_until(), what was asked to be deleted later stays until the test's
//! own event loop turns.
template <typename F>
bool spin_until(F f, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

//! 32-bit float mono, sample[i] = (i + 1) / 1e6: 0 is never a sample
inline void write_ramp_wav(const QString& path, int rate, int frames)
{
  drwav_data_format fmt{};
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  fmt.channels = 1;
  fmt.sampleRate = drwav_uint32(rate);
  fmt.bitsPerSample = 32;
  drwav wav;
  REQUIRE(drwav_init_file_write(&wav, path.toStdString().c_str(), &fmt, nullptr));
  std::vector<float> data(frames);
  for(int i = 0; i < frames; i++)
    data[i] = float((i + 1) * 1e-6);
  REQUIRE(drwav_write_pcm_frames(&wav, frames, data.data()) == drwav_uint64(frames));
  drwav_uninit(&wav);
}

//! The samples of a file, interleaved
inline std::vector<float> read_wav(const QString& path, int& channels)
{
  drwav wav;
  REQUIRE(drwav_init_file(&wav, path.toStdString().c_str(), nullptr));
  channels = wav.channels;
  std::vector<float> v(std::size_t(wav.totalPCMFrameCount) * wav.channels);
  drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, v.data());
  drwav_uninit(&wav);
  return v;
}

}
