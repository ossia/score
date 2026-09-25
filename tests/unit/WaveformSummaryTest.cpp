// The summary is an accelerator: for any range it must give exactly what
// walking every sample gives, bit for bit, on both the mmap and in-RAM
// sources and at every zoom and alignment the renderer asks for.

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/Sound/WaveformComputer.hpp>
#include <Media/WaveformSummary.hpp>

#include <score_test/App.hpp>

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

//! Varied on purpose -- swept tone, exact silence, full scale, single-sample
//! spikes -- so a misplaced bucket boundary shows up as a different min or max.
void make_wav(const QString& path, int64_t frames, int channels, int rate)
{
  const auto u32 = [](QByteArray& o, quint32 v) {
    for(int i = 0; i < 4; i++)
      o.append(char((v >> (8 * i)) & 0xff));
  };
  const auto u16 = [](QByteArray& o, quint16 v) {
    for(int i = 0; i < 2; i++)
      o.append(char((v >> (8 * i)) & 0xff));
  };

  QByteArray pcm;
  pcm.reserve(frames * channels * 2);
  for(int64_t i = 0; i < frames; i++)
  {
    for(int c = 0; c < channels; c++)
    {
      int16_t v{};
      if(i % 7919 == 13)
        v = (c == 0) ? 32767 : -32768; // isolated spikes
      else if(i > frames / 3 && i < frames / 3 + 5000)
        v = 0; // exact silence
      else if(i > frames / 2 && i < frames / 2 + 3000)
        v = (i % 2) ? 32000 : -32000; // full scale
      else
        v = int16_t(
            12000 * std::sin(double(i) * (0.001 + 0.0005 * c) * (1. + double(i) / frames)));
      pcm.append(char(v & 0xff));
      pcm.append(char((v >> 8) & 0xff));
    }
  }

  const quint32 data_size = pcm.size();
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
  out.append(pcm);

  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(out);
}

using Frames = ossia::small_vector<Media::FloatPair, 8>;

//! Compares the summary path against the plain scan over one range.
void check_same(
    Media::AudioFile::ViewHandle& naive, Media::AudioFile::ViewHandle& accel,
    int channels, int64_t start, int64_t end)
{
  Frames a(channels), b(channels);
  naive.minmax_frame(start, end, a);
  accel.minmax_frame(start, end, b);

  for(int c = 0; c < channels; c++)
  {
    INFO(
        "channel " << c << " range [" << start << ", " << end << ") span "
                   << (end - start));
    CHECK(a[c].first == b[c].first);
    CHECK(a[c].second == b[c].second);
  }
}

//! Sweeps a whole file into `columns` pixel columns, the way compute_mean_minmax
//! does, and checks every one.
void check_sweep(
    Media::AudioFile::ViewHandle& naive, Media::AudioFile::ViewHandle& accel,
    int channels, int64_t frames, int columns)
{
  const double spp = double(frames) / columns;
  for(int x = 0; x < columns; x++)
  {
    const int64_t s = int64_t(x * spp);
    const int64_t e = int64_t((x + 1) * spp);
    if(e > frames || e <= s)
      continue;
    check_same(naive, accel, channels, s, e);
  }
}

std::shared_ptr<Media::AudioFile>
load(const QString& path, Media::DecodingMethod method)
{
  auto f = std::make_shared<Media::AudioFile>();
  f->load(Media::DecodingSetup{path, path, method, -1});
  return f;
}
}

TEST_CASE("the waveform summary agrees with a plain scan, mmap source")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/varied.wav";

    constexpr int channels = 2;
    constexpr int rate = 44100;
    constexpr int64_t frames = 300000;
    make_wav(wav, frames, channels, rate);

    auto file = load(wav, Media::DecodingMethod::Mmap);
    REQUIRE(spin_until([&] { return file->finishedDecoding(); }));
    REQUIRE(file->channels() == channels);
    REQUIRE(file->decodedSamples() == frames);

    auto summary = file->waveformSummary();
    REQUIRE(summary);
    CHECK(summary->channels == channels);
    CHECK(summary->frames == frames);
    CHECK(summary->bucket >= 64);
    CHECK(summary->bucketCount == (frames + summary->bucket - 1) / summary->bucket);

    auto naive = file->handle();
    auto accel = file->handle();
    accel.summary = summary;

    const int64_t B = summary->bucket;

    SECTION("ranges the renderer asks for, at several widths")
    {
      for(int columns : {31, 200, 1000})
        check_sweep(naive, accel, channels, frames, columns);
    }

    SECTION("alignments around a bucket boundary")
    {
      // Off-by-one at the ends is what a summary gets wrong.
      for(int64_t off = 0; off < B; off++)
        check_same(naive, accel, channels, 10 * B + off, 10 * B + off + 37 * B);
    }

    SECTION("spans just above and below the threshold for using the summary")
    {
      for(int64_t span : {B, 2 * B, 4 * B - 1, 4 * B, 4 * B + 1, 8 * B})
        for(int64_t start : {int64_t(0), B / 3, 5 * B, 5 * B + 1})
          if(start + span <= frames)
            check_same(naive, accel, channels, start, start + span);
    }

    SECTION("the very ends of the file")
    {
      check_same(naive, accel, channels, 0, 100 * B);
      check_same(naive, accel, channels, frames - 100 * B, frames);
      check_same(naive, accel, channels, 0, frames);
    }
  });
}

TEST_CASE("the waveform summary agrees with a plain scan, in-RAM source")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/varied-ram.wav";

    constexpr int channels = 2;
    constexpr int64_t frames = 200000;
    make_wav(wav, frames, channels, 32000);

    auto file = load(wav, Media::DecodingMethod::Sndfile);
    if(!spin_until([&] { return file->finishedDecoding(); }) || file->channels() == 0)
      SKIP("no in-RAM decoder available in this build");

    // The decoder may resample, so work from what it actually produced.
    const int64_t decoded = file->decodedSamples();
    REQUIRE(decoded > 0);

    auto summary = file->waveformSummary();
    REQUIRE(summary);
    CHECK(summary->frames == decoded);

    auto naive = file->handle();
    auto accel = file->handle();
    accel.summary = summary;
    const int chan = file->channels();

    for(int columns : {17, 150, 800})
      check_sweep(naive, accel, chan, decoded, columns);

    const int64_t B = summary->bucket;
    for(int64_t off = 0; off < B; off += 7)
      if(3 * B + off + 20 * B <= decoded)
        check_same(naive, accel, chan, 3 * B + off, 3 * B + off + 20 * B);
  });
}

TEST_CASE("a source that cannot be summarised keeps drawing", "[waveform]")
{
  // The fast path is never a precondition for producing a waveform.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/plain.wav";
    constexpr int64_t frames = 100000;
    make_wav(wav, frames, 1, 44100);

    auto file = load(wav, Media::DecodingMethod::Mmap);
    REQUIRE(spin_until([&] { return file->finishedDecoding(); }));

    auto h = file->handle();
    CHECK_FALSE(h.summary); // handle() never attaches one

    Frames out(1);
    h.minmax_frame(0, frames, out);
    CHECK(out[0].second > 0.9f); // the full-scale block is in there
    CHECK(out[0].first < -0.9f);
  });
}

TEST_CASE("the summary stays within its memory budget", "[waveform]")
{
  const auto bytes = [](int64_t frames, int32_t ch) {
    const int64_t b = Media::WaveformSummary::bucketFor(frames, ch);
    return ((frames + b - 1) / b) * ch * int64_t(sizeof(Media::FloatPair));
  };

  // A short sound gets the finest bucket; longer ones grow the bucket rather
  // than the table.
  CHECK(Media::WaveformSummary::bucketFor(44100ll * 240, 2) == 64);
  CHECK(Media::WaveformSummary::bucketFor(44100ll * 3600, 2) == 128);
  CHECK(bytes(44100ll * 3600, 2) < 24ll * 1024 * 1024);

  CHECK(Media::WaveformSummary::bucketFor(44100ll * 36000, 2) > 128);
  CHECK(bytes(44100ll * 36000, 2) < 40ll * 1024 * 1024);

  // 32 channels of a long file must not blow up either.
  CHECK(bytes(44100ll * 36000, 32) < 40ll * 1024 * 1024);

  // Always a power of two, so the bucket index is a shift.
  for(int64_t frames : {1000ll, 44100ll * 60, 44100ll * 3600, 44100ll * 100000})
    for(int ch : {1, 2, 8})
    {
      const auto b = Media::WaveformSummary::bucketFor(frames, ch);
      CHECK(b >= 64);
      CHECK((b & (b - 1)) == 0);
    }
}

// Not run by default: `test_unit_waveform_summary "[.bench]"` prints what a
// full-width redraw of a long file costs with and without the summary.
TEST_CASE("waveform summary speedup", "[.bench]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/long.wav";
    constexpr int channels = 2;
    constexpr int64_t frames = 44100ll * 600; // 10 minutes
    make_wav(wav, frames, channels, 44100);

    auto file = load(wav, Media::DecodingMethod::Mmap);
    REQUIRE(spin_until([&] { return file->finishedDecoding(); }));

    QElapsedTimer t;
    t.start();
    auto summary = file->waveformSummary();
    const auto build_ms = t.nsecsElapsed() / 1e6;
    REQUIRE(summary);

    auto naive = file->handle();
    auto accel = file->handle();
    accel.summary = summary;

    const int columns = 1920;
    const auto sweep = [&](Media::AudioFile::ViewHandle& h) {
      Frames out(channels);
      const double spp = double(frames) / columns;
      for(int x = 0; x < columns; x++)
        h.minmax_frame(int64_t(x * spp), int64_t((x + 1) * spp), out);
    };

    t.restart();
    sweep(naive);
    const auto naive_ms = t.nsecsElapsed() / 1e6;

    t.restart();
    for(int i = 0; i < 10; i++)
      sweep(accel);
    const auto accel_ms = t.nsecsElapsed() / 1e6 / 10.;

    WARN(
        "10 min stereo, " << columns << " columns: naive " << naive_ms << " ms, summary "
                          << accel_ms << " ms (build " << build_ms << " ms, "
                          << (summary->bucketCount * channels * sizeof(Media::FloatPair))
                                 / 1048576.
                          << " MB)");
  });
}

// Not run by default: `test_unit_waveform_summary "[.bench]"` reports how often
// an image actually comes back while requests keep arriving, which is what a
// zoom or a drag looks like from the computer's side.
TEST_CASE("waveform render rate under a continuous gesture", "[.bench]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/gesture.wav";
    constexpr int64_t frames = 44100ll * 600;
    make_wav(wav, frames, 2, 44100);

    auto file = load(wav, Media::DecodingMethod::Mmap);
    REQUIRE(spin_until([&] { return file->finishedDecoding(); }));

    Media::Sound::WaveformComputer cpt{false};
    int images = 0;
    QObject::connect(
        &cpt, &Media::Sound::WaveformComputer::ready, &cpt,
        [&](QVector<QImage*> imgs, Media::Sound::ComputedWaveform) {
      images++;
      cpt.claim(imgs);
      Media::Sound::QImagePool::instance().giveBack(imgs);
    });

    // ~5000 samples per pixel: the whole file across a couple of screens.
    Media::Sound::WaveformRequest req{
        file, 8e7, 1., QSizeF{1000., 100.}, 1., 0., 1000.,
        TimeVal{}, TimeVal{}, false, false};

    // Let the summary get built first, so this measures the steady state.
    cpt.recompute(req);
    spin_until([&] { return images > 0; });
    images = 0;

    QElapsedTimer t;
    t.start();
    while(t.elapsed() < 500)
    {
      cpt.recompute(req);
      QApplication::processEvents(QEventLoop::AllEvents, 1);
    }
    const auto ms = t.elapsed();

    WARN(
        images << " images in " << ms << " ms -- one every "
               << (images > 0 ? double(ms) / images : 0.) << " ms");
    CHECK(images > 0);
  });
}
