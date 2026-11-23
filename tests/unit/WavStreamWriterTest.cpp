// The WAV writer of recorded takes: frames are written as they come, the
// sizes at close. Read back with dr_wav, as score reads sound files:
//  - every format (32-bit float, 24-bit, 16-bit), mono, stereo and more
//    channels give back what was written, to the format's precision;
//  - past the RIFF size limit the file becomes RF64 and reads the same;
//  - a take closed with no frame is a valid empty file.

#include <Recording/Takes/WavStreamWriter.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <dr_wav.h>
#include <vector>

using Recording::WavStreamWriter;

namespace
{
float value(int c, int64_t k)
{
  return float(0.8 * std::sin(0.01 * double(k) * (c + 1) + c));
}

//! Writes `frames` frames in blocks of `block`, as a writer thread would.
void write_take(WavStreamWriter& w, int channels, int64_t frames, int64_t block)
{
  std::vector<std::vector<float>> data(channels, std::vector<float>(block));
  std::vector<const float*> ptrs(channels);
  for(int64_t start = 0; start < frames; start += block)
  {
    const int64_t n = std::min(block, frames - start);
    for(int c = 0; c < channels; c++)
    {
      for(int64_t k = 0; k < n; k++)
        data[c][k] = value(c, start + k);
      ptrs[c] = data[c].data();
    }
    REQUIRE(w.write(ptrs.data(), n));
  }
}

struct ReadBack
{
  drwav_uint64 frames{};
  int channels{};
  int rate{};
  drwav_container container{};
  std::vector<float> samples;
};

ReadBack read_back(const std::string& path)
{
  drwav wav;
  REQUIRE(drwav_init_file(&wav, path.c_str(), nullptr));
  ReadBack r;
  r.frames = wav.totalPCMFrameCount;
  r.channels = wav.channels;
  r.rate = int(wav.sampleRate);
  r.container = wav.container;
  r.samples.resize(std::size_t(r.frames) * r.channels);
  CHECK(drwav_read_pcm_frames_f32(&wav, r.frames, r.samples.data()) == r.frames);
  drwav_uninit(&wav);
  return r;
}
}

TEST_CASE("A take written as it comes reads back in every format", "[recording][wav]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  struct F
  {
    WavStreamWriter::Format format;
    float tolerance;
  };
  for(auto [format, tolerance] :
      {F{WavStreamWriter::Format::Float32, 0.f}, F{WavStreamWriter::Format::Pcm24, 2e-7f},
       F{WavStreamWriter::Format::Pcm16, 4e-5f}})
  {
    for(int channels : {1, 2, 3, 6})
    {
      INFO("format " << int(format) << " channels " << channels);
      const std::string path = QString{dir.path() + "/take.wav"}.toStdString();
      WavStreamWriter w;
      REQUIRE(w.open(path, channels, 48000, format));
      // An odd number of frames: 24-bit mono gets a pad byte
      write_take(w, channels, 10001, 512);
      CHECK(w.frames() == 10001);
      REQUIRE(w.close());

      const auto r = read_back(path);
      CHECK(r.container == drwav_container_riff);
      CHECK(r.frames == 10001);
      CHECK(r.channels == channels);
      CHECK(r.rate == 48000);
      int wrong = 0;
      for(int64_t k = 0; k < 10001; k++)
        for(int c = 0; c < channels; c++)
          wrong += std::abs(r.samples[k * channels + c] - value(c, k)) > tolerance;
      CHECK(wrong == 0);
    }
  }
}

TEST_CASE("A take past the RIFF limit becomes RF64", "[recording][wav]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string path = QString{dir.path() + "/long.wav"}.toStdString();

  WavStreamWriter w;
  w.setRiffLimit(20000);
  REQUIRE(w.open(path, 2, 44100, WavStreamWriter::Format::Float32));
  write_take(w, 2, 5000, 1000); // 40000 bytes of data
  REQUIRE(w.close());

  QFile f{QString::fromStdString(path)};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.read(4) == "RF64");
  f.close();

  const auto r = read_back(path);
  CHECK(r.container == drwav_container_rf64);
  CHECK(r.frames == 5000);
  int wrong = 0;
  for(int64_t k = 0; k < 5000; k++)
    for(int c = 0; c < 2; c++)
      wrong += r.samples[k * 2 + c] != value(c, k);
  CHECK(wrong == 0);

  // Below the limit, the same take is a RIFF file
  WavStreamWriter w2;
  REQUIRE(w2.open(path, 2, 44100, WavStreamWriter::Format::Float32));
  write_take(w2, 2, 5000, 1000);
  REQUIRE(w2.close());
  CHECK(read_back(path).container == drwav_container_riff);
}

TEST_CASE("A take closed with no frame is an empty sound file", "[recording][wav]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string path = QString{dir.path() + "/empty.wav"}.toStdString();
  WavStreamWriter w;
  REQUIRE(w.open(path, 2, 48000, WavStreamWriter::Format::Pcm24));
  REQUIRE(w.close());
  drwav wav;
  REQUIRE(drwav_init_file(&wav, path.c_str(), nullptr));
  CHECK(wav.totalPCMFrameCount == 0);
  drwav_uninit(&wav);
}

TEST_CASE("A take that cannot be created says so", "[recording][wav]")
{
  WavStreamWriter w;
  CHECK(!w.open("/nonexistent-directory/take.wav", 2, 48000, WavStreamWriter::Format::Float32));
  CHECK(w.failed());
  CHECK(!w.isOpen());
}
