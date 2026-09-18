// Positioning the libav streaming sound node: seeks have to land where they
// are asked to, and fetch_audio() has to read from its `start` argument rather
// than from wherever the demuxer happens to sit.
//
// The fixture is a ramp, sample[i] == i / frames, so a sample read back names
// the file frame it came from.

#include <ossia/dataflow/nodes/sound_libav.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>
#include <ossia/detail/thread.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr int rate = 44100;
constexpr int64_t frames = int64_t(rate) * 10;

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

//! 32-bit float mono, sample[i] = i / frames.
void make_ramp(const QString& path)
{
  const quint32 data_size = quint32(frames * 4);
  QByteArray out;
  out.append("RIFF");
  u32(out, 36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(out, 16);
  u16(out, 3); // IEEE float
  u16(out, 1);
  u32(out, rate);
  u32(out, rate * 4);
  u16(out, 4);
  u16(out, 32);
  out.append("data");
  u32(out, data_size);

  out.reserve(out.size() + data_size);
  for(int64_t i = 0; i < frames; i++)
  {
    const float v = float(double(i) / double(frames));
    const char* p = reinterpret_cast<const char*>(&v);
    out.append(p, 4);
  }

  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(out) == out.size());
}

//! The file frame a sample value came from.
double frame_of(double value)
{
  return value * double(frames);
}
} // namespace

TEST_CASE("a libav seek lands where it was asked to", "[libav][seek]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/ramp.wav";
  make_ramp(path);

  ossia::libav_handle h;
  h.open(path.toStdString(), 0, 0);
  REQUIRE(bool(h));
  REQUIRE(h.rate() == rate);

  auto* packet = av_packet_alloc();
  auto* frame = av_frame_alloc();

  //! Seeks, then returns the timestamp of the first frame decoded after it.
  auto seek_and_tell = [&](double seconds) -> int64_t {
    const int64_t flicks = int64_t(ossia::flicks_per_second<double> * seconds);
    if(!ossia::seek_to_flick(h.format, h.codec, h.stream, flicks, AVSEEK_FLAG_BACKWARD))
      return -1;

    while(av_read_frame(h.format, packet) >= 0)
    {
      if(packet->stream_index != h.stream->index)
      {
        av_packet_unref(packet);
        continue;
      }
      if(avcodec_send_packet(h.codec, packet) == 0
         && avcodec_receive_frame(h.codec, frame) == 0)
      {
        const int64_t pts = frame->best_effort_timestamp;
        av_packet_unref(packet);
        return pts;
      }
      av_packet_unref(packet);
    }
    return -1;
  };

  // Landing at or just before the target is the contract.
  for(double seconds : {1.0, 5.0, 9.0, 2.5})
  {
    const int64_t pts = seek_and_tell(seconds);
    const int64_t want = int64_t(seconds * rate);
    INFO("seeking to " << seconds << " s, i.e. frame " << want);
    REQUIRE(pts >= 0);
    CHECK(pts <= want);
    CHECK(want - pts < 8192);
  }

  av_frame_free(&frame);
  av_packet_free(&packet);
}

TEST_CASE("sound_libav reads from where it is asked", "[libav][seek]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/ramp.wav";
  make_ramp(path);

  ossia::libav_handle h;
  h.open(path.toStdString(), 0, rate);
  REQUIRE(bool(h));

  ossia::nodes::sound_libav snd;
  snd.set_sound(std::move(h));

  constexpr int N = 128;
  std::vector<double> buf(N);
  double* out[1] = {buf.data()};

  //! Reads N samples at `start` and says which file frame they came from.
  auto read_at = [&](int64_t start) {
    std::fill(buf.begin(), buf.end(), -1.);
    snd.fetch_audio(start, N, out);
    return frame_of(buf[0]);
  };

  SECTION("sequentially, as raw_stretcher walks it")
  {
    for(int i = 0; i < 6; i++)
      CHECK(read_at(int64_t(i) * N) == Catch::Approx(double(i * N)).margin(1.0));
  }

  SECTION("after a jump forward")
  {
    CHECK(read_at(rate) == Catch::Approx(double(rate)).margin(1.0));
    CHECK(read_at(5 * rate) == Catch::Approx(double(5 * rate)).margin(1.0));
  }

  SECTION("after a jump backward")
  {
    read_at(5 * rate);
    CHECK(read_at(rate) == Catch::Approx(double(rate)).margin(1.0));
    CHECK(read_at(0) == Catch::Approx(0.).margin(1.0));
  }

  SECTION("re-reading just behind, as repitch does every buffer")
  {
    // src_process consumes fewer frames than it is handed, so the next fetch
    // asks for a position inside what the last one returned.
    const int64_t base = 40 * N;
    read_at(base);
    for(int back : {17, 64, 100, 127})
    {
      const int64_t s = base + N - back;
      INFO("re-reading " << back << " samples behind");
      CHECK(read_at(s) == Catch::Approx(double(s)).margin(1.0));
    }
  }

  SECTION("at arbitrary positions")
  {
    for(int64_t s : {int64_t(1), int64_t(4097), int64_t(123457), int64_t(7 * rate + 13),
                     int64_t(3 * rate)})
    {
      INFO("start " << s);
      CHECK(read_at(s) == Catch::Approx(double(s)).margin(1.0));
    }
  }

  SECTION("every sample of a block, not just its first")
  {
    constexpr int64_t s = 2 * rate + 555;
    std::fill(buf.begin(), buf.end(), -1.);
    snd.fetch_audio(s, N, out);
    for(int k = 0; k < N; k++)
    {
      INFO("offset " << k);
      REQUIRE(frame_of(buf[k]) == Catch::Approx(double(s + k)).margin(1.0));
    }
  }

  SECTION("backward playback reads the span ending at start, reversed")
  {
    constexpr int64_t s = 6 * rate;
    std::fill(buf.begin(), buf.end(), -1.);
    snd.fetch_audio_backward(s, N, out);

    // buf[0] is `start`, walking down to start - N + 1.
    for(int k = 0; k < N; k++)
    {
      INFO("offset " << k);
      REQUIRE(frame_of(buf[k]) == Catch::Approx(double(s - k)).margin(1.0));
    }
  }

  SECTION("past the end gives silence rather than whatever was there")
  {
    std::fill(buf.begin(), buf.end(), -1.);
    snd.fetch_audio(frames + 10 * rate, N, out);
    for(int k = 0; k < N; k++)
      REQUIRE(buf[k] == 0.);
  }
}

TEST_CASE("sound_libav positions in the rate it outputs at", "[libav][seek]")
{
  // The handle resamples to the graph's rate, so positions count in that rate.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/ramp.wav";
  make_ramp(path);

  constexpr int graph_rate = 48000;
  ossia::libav_handle h;
  h.open(path.toStdString(), 0, graph_rate);
  REQUIRE(bool(h));
  CHECK(h.rate() == rate);
  CHECK(h.out_rate() == graph_rate);

  ossia::nodes::sound_libav snd;
  snd.set_sound(std::move(h));
  // Duration is in the output rate too, or it will not line up with them.
  CHECK(
      double(snd.duration())
      == Catch::Approx(double(frames) * graph_rate / rate).margin(64.));

  constexpr int N = 128;
  std::vector<double> buf(N);
  double* out[1] = {buf.data()};

  for(int64_t s : {int64_t(0), int64_t(graph_rate), int64_t(5 * graph_rate),
                   int64_t(123457)})
  {
    std::fill(buf.begin(), buf.end(), -1.);
    snd.fetch_audio(s, N, out);
    // Output sample s corresponds to file frame s * rate / graph_rate.
    const double want = double(s) * rate / graph_rate;
    INFO("output sample " << s);
    CHECK(frame_of(buf[0]) == Catch::Approx(want).margin(2.0));
  }
}
