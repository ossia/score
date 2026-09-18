// Which backend AudioFile::load() ends up on, and that each either produces a
// usable handle or fails cleanly rather than storing a half-built one.

#include <Media/AudioDecoder.hpp>
#include <Media/MediaFileHandle.hpp>

#include <score_test/App.hpp>

#include <ossia/detail/libav.hpp>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

#include <cmath>

namespace
{
template <typename F>
bool spin_until(F f, int ms = 20000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

void u32(QByteArray& o, quint32 v)
{
  for(int i = 0; i < 4; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}
void u16(QByteArray& o, quint16 v)
{
  for(int i = 0; i < 2; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}

QByteArray wav_header(int64_t frames, int channels, int rate)
{
  const quint32 data_size = quint32(frames * channels * 2);
  QByteArray out;
  out.append("RIFF");
  u32(out, 36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(out, 16);
  u16(out, 1);
  u16(out, channels);
  u32(out, rate);
  u32(out, rate * channels * 2);
  u16(out, channels * 2);
  u16(out, 16);
  out.append("data");
  u32(out, data_size);
  return out;
}

//! A real 16-bit PCM wav of a sine.
void make_wav(const QString& path, int64_t frames, int channels, int rate)
{
  QByteArray out = wav_header(frames, channels, rate);
  out.reserve(out.size() + frames * channels * 2);
  for(int64_t i = 0; i < frames; i++)
    for(int c = 0; c < channels; c++)
    {
      const auto v = int16_t(12000 * std::sin(double(i) * 0.06));
      out.append(char(v & 0xff));
      out.append(char((v >> 8) & 0xff));
    }

  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(out) == out.size());
}

//! Honest header, sparse samples: over load_libav_stream()'s RAM threshold
//! without costing the disk.
void make_sparse_wav(const QString& path, int64_t frames, int channels, int rate)
{
  const QByteArray hdr = wav_header(frames, channels, rate);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(hdr) == hdr.size());
  REQUIRE(f.resize(hdr.size() + frames * channels * 2));
}

std::shared_ptr<Media::AudioFile>
load(const QString& path, Media::DecodingMethod method)
{
  auto f = std::make_shared<Media::AudioFile>();
  f->load(Media::DecodingSetup{path, path, method, -1});
  return f;
}
} // namespace

TEST_CASE("a wav probe names its audio stream")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    make_wav(wav, 44100, 1, 44100);

    const auto info = Media::probe(wav);
    REQUIRE(info);
    // -1 means unknown, and libav rejects it; stream 0 is not the same answer.
    CHECK(info->audioStream == 0);
    CHECK(info->fileRate == 44100);
    CHECK((info->flags & Media::AudioInfo::DrwavCanDecode) != 0);
  });
}

TEST_CASE("a probed stream index opens with libav")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    make_wav(wav, 44100, 2, 48000);

    const auto info = Media::probe(wav);
    REQUIRE(info);

    ossia::libav_handle h;
    h.open(wav.toStdString(), info->audioStream, 0);
    REQUIRE(bool(h));
    CHECK(h.channels() == 2);
    CHECK(h.rate() == 48000);

    ossia::libav_handle bad;
    bad.open(wav.toStdString(), -1, 0);
    CHECK(!bool(bad));
  });
}

TEST_CASE("a large wav streams through libav instead of going silent")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/big.wav";

    // Over load_libav_stream()'s 100 MB threshold for streaming vs. RAM.
    constexpr int64_t frames = 20'000'000;
    make_sparse_wav(wav, frames, 2, 44100);

    auto file = load(wav, Media::DecodingMethod::LibavStream);
    REQUIRE(spin_until([&] { return file->finishedDecoding(); }));

    REQUIRE(!file->empty());
    CHECK(file->channels() == 2);
    CHECK(file->samples() == frames);
    CHECK(file->sampleRate() == 44100);
  });
}

TEST_CASE("a wav is mmapped whatever its rate")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    // The graph converts the rate, so both map instead of one being decoded.
    for(int rate : {44100, 48000})
    {
      const QString wav = dir.path() + QString("/tone%1.wav").arg(rate);
      make_wav(wav, 44100, 1, rate);

      auto file = load(wav, Media::DecodingMethod::Invalid);
      REQUIRE(spin_until([&] { return file->finishedDecoding(); }));

      INFO("file rate " << rate);
      REQUIRE(!file->empty());
      CHECK(file->sampleRate() == rate);
      CHECK(file->unsafe_handle().target<Media::AudioFile::MmapReader>() != nullptr);
    }
  });
}

TEST_CASE("an unreadable file fails without taking the handle with it")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("a file drwav cannot parse")
    {
      const QString path = dir.path() + "/garbage.wav";
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(QByteArray(4096, '\x7f'));
      f.close();

      auto file = load(path, Media::DecodingMethod::Mmap);
      CHECK(file->empty());
      CHECK(file->channels() == 0);
      CHECK(file->samples() == 0);
      // Or every waveform waiting on it waits forever.
      CHECK(file->finishedDecoding());
    }

    SECTION("a header with no samples behind it")
    {
      const QString path = dir.path() + "/empty.wav";
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.close();

      auto file = load(path, Media::DecodingMethod::Mmap);
      CHECK(file->empty());
      CHECK(file->finishedDecoding());
    }

    SECTION("a file that is not there at all")
    {
      auto file = load(dir.path() + "/nope.wav", Media::DecodingMethod::Mmap);
      CHECK(file->empty());
      CHECK(file->finishedDecoding());
    }
  });
}
