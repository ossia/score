// AudioDecoder::decode_synchronous() hands back exactly the frames of the file,
// at the file's rate and resampled either way: neither the end of the file cut
// off (upsampling outgrowing buffers sized for the file's rate) nor zeros
// appended (downsampling into them), which a sampler looping the whole file
// would play at every turn.

#include <Media/AudioDecoder.hpp>

#include <score_test/App.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <cstdlib>

namespace
{
//! A 16-bit PCM mono wav of a sine at a quarter of full scale.
void make_wav(const QString& path, int64_t frames, int rate)
{
  QByteArray out;
  const auto u32 = [&](quint32 v) {
    for(int i = 0; i < 4; i++)
      out.append(char((v >> (8 * i)) & 0xff));
  };
  const auto u16 = [&](quint16 v) {
    for(int i = 0; i < 2; i++)
      out.append(char((v >> (8 * i)) & 0xff));
  };
  const quint32 data_size = quint32(frames * 2);
  out.append("RIFF");
  u32(36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(16);
  u16(1);
  u16(1);
  u32(rate);
  u32(rate * 2);
  u16(2);
  u16(16);
  out.append("data");
  u32(data_size);
  for(int64_t i = 0; i < frames; i++)
  {
    const auto v = int16_t(8192 * std::sin(2. * M_PI * 441. * double(i) / rate));
    u16(quint16(v));
  }

  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(out) == out.size());
}

template <typename Channel>
double rms(const Channel& c, std::size_t from, std::size_t to)
{
  double sum = 0.;
  for(std::size_t i = from; i < to; i++)
    sum += double(c[i]) * double(c[i]);
  return std::sqrt(sum / double(to - from));
}

void check(int64_t frames, int fileRate, int rate)
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString wav = dir.path() + "/tone.wav";
  make_wav(wav, frames, fileRate);

  auto res = Media::AudioDecoder::decode_synchronous(wav, rate);
  REQUIRE(res);
  auto& data = res->second;
  REQUIRE(data.size() == 1);
  const auto& c = data[0];

  const double expected = double(frames) * rate / fileRate;
  INFO(frames << " frames at " << fileRate << " Hz decoded at " << rate << " Hz: "
               << c.size() << " frames, " << expected << " expected");
  if(rate == fileRate)
    CHECK(int64_t(c.size()) == frames);
  else
    CHECK(std::abs(double(c.size()) - expected) <= 2.);

  // The sine goes on to the end: no padding, no truncation filled with zeros
  REQUIRE(c.size() > 400);
  const double amplitude = 0.25 / std::sqrt(2.);
  CHECK(rms(c, c.size() / 2 - 200, c.size() / 2) == Catch::Approx(amplitude).epsilon(0.05));
  CHECK(rms(c, c.size() - 300, c.size() - 100) == Catch::Approx(amplitude).epsilon(0.05));
  bool silentEnd = true;
  for(std::size_t i = c.size() - 16; i < c.size(); i++)
    silentEnd = silentEnd && c[i] == 0.f;
  CHECK_FALSE(silentEnd);
}
}

TEST_CASE("a file decoded at its own rate keeps its exact length", "[media][decoder]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    check(44100, 44100, 44100);
    check(12345, 44100, 44100);
    check(48001, 48000, 48000);
  });
}

TEST_CASE("a resampled file keeps its duration", "[media][decoder]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Up: the buffers sized for the file's rate were too short
    check(44100, 44100, 48000);
    check(12345, 44100, 96000);
    // Down: the rest of the buffers stayed as zeros
    check(48000, 48000, 44100);
    check(96001, 96000, 22050);
  });
}
