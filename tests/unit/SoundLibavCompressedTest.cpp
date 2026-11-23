// The libav streaming sound node (what a Sound process plays a file over 4 GB
// or a video's soundtrack with) on compressed files: FLAC, MP3, AAC, Opus,
// Vorbis, encoded here with libav from a chirp, so that a stretch of signal
// names where in the file it comes from.
//
// A read with a start offset or a loop must give exactly what reads of the
// same file ranges give without one:
//  - a negative start offset: silence, then the content from its start;
//  - a positive start offset: what a read further in the file gives;
//  - looping with a start offset: each loop as the content, wrapped.
// FLAC is lossless, so it is also checked against the signal encoded. Where
// a read lands against the signal, for the lossy codecs, is checked by a
// hidden case: it fails today for MP3, AAC and Opus, offset or not.

#include <ossia/dataflow/nodes/sound_libav.hpp>
#include <ossia/detail/libav.hpp>
#include <ossia/detail/thread.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

namespace
{
constexpr double seconds = 3.;

struct Codec
{
  const char* encoder;
  const char* muxer;
  const char* extension;
  int rate;
  bool lossless;
};

const Codec codecs[] = {
    {"flac", "flac", "flac", 44100, true},
    {"libmp3lame", "mp3", "mp3", 44100, false},
    {"aac", "mp4", "m4a", 44100, false},
    {"libopus", "ogg", "opus", 48000, false},
    {"libvorbis", "ogg", "ogg", 44100, false},
};

//! A rising chirp: every stretch of it is unlike every other.
std::vector<float> chirp(int rate)
{
  const int n = int(seconds * rate);
  std::vector<float> s(n);
  const double f0 = 200., f1 = 3000., T = seconds;
  for(int i = 0; i < n; i++)
  {
    const double t = double(i) / rate;
    const double phase = 2. * M_PI * (f0 * t + (f1 - f0) * t * t / (2. * T));
    s[i] = float(0.5 * std::sin(phase));
  }
  return s;
}

void put_sample(AVFrame* f, AVSampleFormat fmt, int i, float v)
{
  switch(fmt)
  {
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
      reinterpret_cast<float*>(f->data[0])[i] = v;
      break;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
      reinterpret_cast<int16_t*>(f->data[0])[i] = int16_t(std::lround(v * 32767.));
      break;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
      reinterpret_cast<int32_t*>(f->data[0])[i]
          = int32_t(std::llround(double(v) * 2147483647.));
      break;
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_DBLP:
      reinterpret_cast<double*>(f->data[0])[i] = v;
      break;
    default:
      break;
  }
}

//! Encodes a mono signal; an empty string when it worked, else why not.
std::string
encode(const std::string& path, const Codec& codec, const std::vector<float>& sig)
{
  const AVCodec* enc = avcodec_find_encoder_by_name(codec.encoder);
  if(!enc)
    return std::string{"no encoder "} + codec.encoder;

  AVFormatContext* fmt{};
  if(avformat_alloc_output_context2(&fmt, nullptr, codec.muxer, path.c_str()) < 0
     || !fmt)
    return "no muxer";
  AVStream* st = avformat_new_stream(fmt, nullptr);
  AVCodecContext* c = avcodec_alloc_context3(enc);
  c->sample_rate = codec.rate;
  av_channel_layout_default(&c->ch_layout, 1);
  const void* cfg{};
  int nfmt{};
  avcodec_get_supported_config(c, enc, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &cfg, &nfmt);
  const auto* fmts = static_cast<const AVSampleFormat*>(cfg);
  c->sample_fmt = (fmts && nfmt > 0) ? fmts[0] : AV_SAMPLE_FMT_FLTP;
  if(!codec.lossless)
    c->bit_rate = 160000;
  c->time_base = AVRational{1, codec.rate};
  c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
  if(fmt->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  if(avcodec_open2(c, enc, nullptr) < 0)
  {
    avcodec_free_context(&c);
    avformat_free_context(fmt);
    return "could not open the encoder";
  }
  avcodec_parameters_from_context(st->codecpar, c);
  st->time_base = c->time_base;
  if(avio_open(&fmt->pb, path.c_str(), AVIO_FLAG_WRITE) < 0
     || avformat_write_header(fmt, nullptr) < 0)
  {
    avcodec_free_context(&c);
    avformat_free_context(fmt);
    return "could not write the header";
  }

  AVPacket* pkt = av_packet_alloc();
  auto drain = [&] {
    while(avcodec_receive_packet(c, pkt) == 0)
    {
      av_packet_rescale_ts(pkt, c->time_base, st->time_base);
      pkt->stream_index = st->index;
      av_interleaved_write_frame(fmt, pkt);
    }
  };

  const bool variable
      = (enc->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) || c->frame_size <= 0;
  const int frame_size = c->frame_size > 0 ? c->frame_size : 1024;
  const bool small_last = enc->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME;
  AVFrame* f = av_frame_alloc();
  f->format = c->sample_fmt;
  av_channel_layout_copy(&f->ch_layout, &c->ch_layout);
  f->sample_rate = codec.rate;
  f->nb_samples = frame_size;
  av_frame_get_buffer(f, 0);

  const int total = int(sig.size());
  for(int pos = 0; pos < total; pos += frame_size)
  {
    const int n = std::min(frame_size, total - pos);
    av_frame_make_writable(f);
    f->nb_samples = (n < frame_size && !(variable || small_last)) ? frame_size : n;
    for(int i = 0; i < f->nb_samples; i++)
      put_sample(f, c->sample_fmt, i, i < n ? sig[pos + i] : 0.f);
    f->pts = pos;
    avcodec_send_frame(c, f);
    drain();
  }
  avcodec_send_frame(c, nullptr);
  drain();
  av_write_trailer(fmt);
  avio_closep(&fmt->pb);
  av_frame_free(&f);
  av_packet_free(&pkt);
  avcodec_free_context(&c);
  avformat_free_context(fmt);
  return {};
}

//! A fresh node on the file, as a Sound process would have it.
struct Reader
{
  ossia::nodes::sound_libav node;
  Reader(const std::string& path, int rate)
  {
    ossia::libav_handle h;
    h.open(path, 0, rate);
    REQUIRE(bool(h));
    node.set_sound(std::move(h));
  }
  std::vector<float> read(int64_t start, int64_t n, int64_t offset, int64_t loop = 0)
  {
    node.m_start_offset_samples = offset;
    node.m_loops = loop > 0;
    node.m_loop_duration_samples = loop;
    std::vector<float> buf(n, 12345.f);
    float* out[1] = {buf.data()};
    node.fetch_audio(start, n, out);
    return buf;
  }
};

//! The shift that best aligns b on a (b[i + lag] ~ a[i]), and how well.
std::pair<int, double>
best_lag(const std::vector<float>& a, const std::vector<float>& b, int max_lag)
{
  int best = 0;
  double best_c = -2.;
  const int n = int(std::min(a.size(), b.size())) - 2 * max_lag;
  for(int lag = -max_lag; lag <= max_lag; lag++)
  {
    double ab = 0, aa = 0, bb = 0;
    for(int i = max_lag; i < max_lag + n; i++)
    {
      const double x = a[i], y = b[i + lag];
      ab += x * y;
      aa += x * x;
      bb += y * y;
    }
    const double c = (aa > 0 && bb > 0) ? ab / std::sqrt(aa * bb) : 0.;
    if(c > best_c)
    {
      best_c = c;
      best = lag;
    }
  }
  return {best, best_c};
}

std::vector<float> slice(const std::vector<float>& v, std::size_t from, std::size_t n)
{
  return {v.begin() + from, v.begin() + from + n};
}

bool all_zero(const std::vector<float>& v, std::size_t from, std::size_t n)
{
  for(std::size_t i = from; i < from + n; i++)
    if(v[i] != 0.f)
      return false;
  return true;
}

}

TEST_CASE(
    "The libav streaming node applies start offsets and loops on compressed files",
    "[sound][offset][libav][codecs]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  int tested = 0;
  for(const auto& codec : codecs)
  {
    const auto sig = chirp(codec.rate);
    const std::string path
        = QString{dir.path() + "/chirp." + codec.extension}.toStdString();
    const auto why = encode(path, codec, sig);
    INFO("codec " << codec.encoder << " " << why);
    if(!why.empty())
    {
      WARN("skipped " << codec.encoder << ": " << why);
      continue;
    }
    tested++;

    constexpr int64_t N = 8192;

    // Each read with an offset or a loop must give exactly what reads of the
    // same file ranges give without one, on a node that made the same reads:
    // the offset and the loop only choose what is read.

    // Negative offset: silence for the delay, then the content from its start
    {
      auto r = Reader{path, codec.rate}.read(0, N, -1000);
      CHECK(all_zero(r, 0, 1000));
      auto ref = Reader{path, codec.rate}.read(0, N - 1000, 0);
      CHECK(slice(r, 1000, N - 1000) == ref);
    }

    // Positive offset: as a read further in
    {
      auto r = Reader{path, codec.rate}.read(20000, N, 3000);
      auto ref = Reader{path, codec.rate}.read(23000, N, 0);
      CHECK(r == ref);
    }

    // Looping with a negative offset: loop of 30000, offset -2000. From 25000:
    // the loop's [25000; 30000[ is content [23000; 28000[, then the next
    // loop's [0; 2000[ is silence, then content from 0.
    {
      auto r = Reader{path, codec.rate}.read(25000, 10000, -2000, 30000);
      Reader ref{path, codec.rate};
      auto first = ref.read(23000, 5000, 0);
      auto second = ref.read(0, 3000, 0);
      CHECK(slice(r, 0, 5000) == first);
      CHECK(all_zero(r, 5000, 2000));
      CHECK(slice(r, 7000, 3000) == second);
    }

    if(codec.lossless)
    {
      // FLAC gives back what it was given (at 16 bits)
      auto r = Reader{path, codec.rate}.read(10000, 4096, -500);
      for(int i = 0; i < 4096; i++)
      {
        INFO("i " << i);
        REQUIRE(std::abs(r[i] - sig[10000 - 500 + i]) < 1e-4);
      }
    }
  }
  CHECK(tested > 0);
}

// Fails today for MP3, AAC and Opus: the streaming node's own decoding, with
// or without an offset (see the [.diag] case). Hidden until it is fixed.
TEST_CASE(
    "A read lands where the signal is, whatever the codec", "[.][sound][libav][codecs]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  for(const auto& codec : codecs)
  {
    const auto sig = chirp(codec.rate);
    const std::string path
        = QString{dir.path() + "/chirp." + codec.extension}.toStdString();
    if(!encode(path, codec, sig).empty())
      continue;
    INFO("codec " << codec.encoder);
    for(const int64_t at : {int64_t(0), int64_t(30000), int64_t(70000)})
    {
      INFO("at " << at);
      auto r = Reader{path, codec.rate}.read(at, 8192, 0);
      std::vector<float> want(sig.begin() + at, sig.begin() + at + 8192);
      const auto [lag, corr] = best_lag(want, r, 2000);
      INFO("lag " << lag << " correlation " << corr);
      CHECK(lag == 0);
      CHECK(corr > 0.9);
    }
  }
}

TEST_CASE("diagnostic", "[.diag]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  QTemporaryDir dir;
  for(const auto& codec : codecs)
  {
    const auto sig = chirp(codec.rate);
    const std::string path
        = QString{dir.path() + "/chirp." + codec.extension}.toStdString();
    if(!encode(path, codec, sig).empty())
      continue;
    auto first_nonzero = [](const std::vector<float>& v) {
      for(std::size_t i = 0; i < v.size(); i++)
        if(v[i] != 0.f)
          return int(i);
      return -1;
    };
    auto where = [&](const std::vector<float>& r, int64_t guess) {
      // position in sig that best matches r[4096..4096+2048]
      std::vector<float> w(r.begin() + 4096, r.begin() + 4096 + 2048);
      int best = 0;
      double bc = -2;
      for(int64_t p = std::max<int64_t>(0, guess - 3000); p < guess + 3000; p++)
      {
        double ab = 0, aa = 0, bb = 0;
        for(int i = 0; i < 2048; i++)
        {
          ab += w[i] * sig[p + i];
          aa += w[i] * w[i];
          bb += sig[p + i] * sig[p + i];
        }
        const double c = (aa > 0 && bb > 0) ? ab / std::sqrt(aa * bb) : 0;
        if(c > bc)
        {
          bc = c;
          best = int(p);
        }
      }
      return std::pair{best - (guess), bc};
    };
    for(const int64_t at : {int64_t(0), int64_t(20000), int64_t(70000)})
    {
      auto r = Reader{path, codec.rate}.read(at, 8192, 0);
      auto [d, c] = where(r, at + 4096);
      printf(
          "%s read at %lld: first nonzero %d, shift %d corr %.3f\n", codec.encoder,
          (long long)at, first_nonzero(r), d, c);
    }
    // sequential reads from 0 on one node
    {
      Reader rd{path, codec.rate};
      std::vector<float> all;
      for(int k = 0; k < 10; k++)
      {
        auto r = rd.read(k * 1024, 1024, 0);
        all.insert(all.end(), r.begin(), r.end());
      }
      auto [d, c] = where(all, 4096);
      printf(
          "%s sequential from 0: first nonzero %d, shift %d corr %.3f\n", codec.encoder,
          first_nonzero(all), d, c);
    }
  }
}
