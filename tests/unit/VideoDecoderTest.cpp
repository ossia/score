// LibAVDecoder::enqueue_frame(): the send_packet/receive_frame loop every
// video decode path in score goes through (VideoDecoder for files,
// LibavStreamInput for streams).
//
// The contract under test is FFmpeg's own (libavcodec/decode.c):
//  - avcodec_send_packet returns EAGAIN when its one-packet buffer is still
//    occupied, and that guarantees avcodec_receive_frame yields a frame.
//    The packet REFUSED by send_packet was not consumed: it must be retried
//    after draining, never dropped. Dropping it breaks the reference chains
//    of every inter-coded stream (H.264/HEVC/MPEG...).
//  - One packet may make several frames available; all of them must come
//    out, in decode order.
//  - A frame that decodes but is discarded by policy (negative pts) still
//    made room in the codec: it is progress, not a stop condition.
//
// The tests synthesize real encoded streams in-process with libavcodec, put
// the decoder into the exact backpressure state (send twice without
// receiving: the first packet decodes into the codec's buffer_frame, the
// second sits in buffer_pkt, the third send returns EAGAIN) and assert the
// N-in / N-out / in-order invariant.

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV

#include <Video/PlaybackTime.hpp>
#include <Video/VideoDecoder.hpp>

#include <ossia/detail/libav.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace
{
// A path in the temp directory no other run uses: test jobs run in parallel.
inline std::filesystem::path uniqueTempPath(const std::string& name)
{
  static std::atomic_int counter{};
  return std::filesystem::temp_directory_path()
         / (name + "_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "_" + std::to_string(counter++));
}

struct Packets
{
  std::vector<AVPacket*> pkts;

  ~Packets()
  {
    for(auto* p : pkts)
      av_packet_free(&p);
  }
};

struct EncoderSettings
{
  AVCodecID codec{AV_CODEC_ID_MPEG1VIDEO};
  int frames{12};
  int gop_size{1};
  int max_b_frames{0};
  // What a muxer with AVFMT_GLOBALHEADER (mp4...) requires of its encoder.
  bool global_header{false};
  //! Frames per second, and the encoder's time base 1/rate.
  int rate{25};
};

constexpr int W = 64;
constexpr int H = 48;

// Encode `frames` yuv420p pictures, one packet per call order, pts 0..N-1 in
// a 1/rate time base.
static void encode(const EncoderSettings& s, Packets& out, AVCodecContext** keep_par)
{
  const AVCodec* codec = avcodec_find_encoder(s.codec);
  REQUIRE(codec);

  AVCodecContext* ctx = avcodec_alloc_context3(codec);
  REQUIRE(ctx);
  ctx->width = W;
  ctx->height = H;
  ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  ctx->time_base = {1, s.rate};
  ctx->framerate = {s.rate, 1};
  ctx->bit_rate = 400000;
  ctx->gop_size = s.gop_size;
  ctx->max_b_frames = s.max_b_frames;
  if(s.global_header)
    ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  REQUIRE(avcodec_open2(ctx, codec, nullptr) == 0);

  AVFrame* frame = av_frame_alloc();
  frame->format = ctx->pix_fmt;
  frame->width = ctx->width;
  frame->height = ctx->height;
  REQUIRE(av_frame_get_buffer(frame, 0) == 0);

  AVPacket* pkt = av_packet_alloc();

  auto pump = [&](AVFrame* f) {
    REQUIRE(avcodec_send_frame(ctx, f) == 0);
    for(;;)
    {
      int ret = avcodec_receive_packet(ctx, pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      REQUIRE(ret == 0);
      // One frame per tick of the 1/rate time base. Without a duration the mp4
      // muxer ends the track's edit list one frame early and the demuxer
      // flags the last packet AV_PKT_FLAG_DISCARD: its frame then never
      // leaves libavcodec, by design.
      pkt->duration = 1;
      out.pkts.push_back(av_packet_clone(pkt));
      av_packet_unref(pkt);
    }
  };

  for(int i = 0; i < s.frames; i++)
  {
    REQUIRE(av_frame_make_writable(frame) == 0);
    // A moving gradient so every frame differs
    for(int y = 0; y < H; y++)
      for(int x = 0; x < W; x++)
        frame->data[0][y * frame->linesize[0] + x] = uint8_t(x + y + i * 3);
    for(int y = 0; y < H / 2; y++)
      for(int x = 0; x < W / 2; x++)
      {
        frame->data[1][y * frame->linesize[1] + x] = uint8_t(128 + i);
        frame->data[2][y * frame->linesize[2] + x] = uint8_t(64 + x);
      }
    frame->pts = i;
    pump(frame);
  }
  pump(nullptr); // flush the encoder

  av_packet_free(&pkt);
  av_frame_free(&frame);

  REQUIRE(int(out.pkts.size()) == s.frames);
  *keep_par = ctx;
}

// A LibAVDecoder whose codec context decodes what `enc` produced, plus the
// AVFormatContext/AVStream scaffolding init_codec_context needs.
struct TestDecoder
{
  Video::LibAVDecoder dec;
  AVFormatContext* fmt{};

  // threads == 1: single-threaded, so the buffer_pkt / buffer_frame state
  // machine of libavcodec is exactly the one the test drives into
  // backpressure. threads == 0: libavcodec auto frame threading, which gives
  // the decoder a delay of many frames.
  explicit TestDecoder(AVCodecContext* enc, bool ignorePts, int threads = 1)
  {
    dec.m_conf.useAVCodec = true;
    dec.m_conf.ignorePTS = ignorePts;
    dec.m_conf.threads = threads;

    fmt = avformat_alloc_context();
    REQUIRE(fmt);
    AVStream* st = avformat_new_stream(fmt, nullptr);
    REQUIRE(st);
    REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
    st->time_base = enc->time_base;

    const AVCodec* codec = avcodec_find_decoder(enc->codec_id);
    REQUIRE(codec);
    REQUIRE(
        dec.init_codec_context(codec, nullptr, st, [st](AVCodecContext& ctx) {
      ctx.pkt_timebase = st->time_base;
        }) == 0);
    dec.m_avstream = st;
  }

  ~TestDecoder()
  {
    if(dec.m_codecContext)
      avcodec_free_context(&dec.m_codecContext);
    dec.m_frames.drain();
    avformat_free_context(fmt);
  }

  // Everything a call to enqueue_frame made available, in order: first what
  // it put in the queue itself, then the frame it returned (which the real
  // callers enqueue right after).
  void collect(Video::ReadFrame r, std::vector<int64_t>& pts)
  {
    while(AVFrame* f = dec.m_frames.dequeue_one())
    {
      pts.push_back(f->pts);
      av_frame_free(&f);
    }
    if(r.frame)
    {
      pts.push_back(r.frame->pts);
      av_frame_free(&r.frame);
    }
  }
};

// Send packets straight into libavcodec until one is refused with EAGAIN.
// Returns the index of the refused packet — the state the old code hit and
// mishandled. With a one-in-one-out intra codec this refusal happens on the
// third packet: the first decoded into buffer_frame, the second parked in
// buffer_pkt.
static size_t preload_until_backpressure(AVCodecContext* ctx, const Packets& p)
{
  for(size_t i = 0; i < p.pkts.size(); i++)
  {
    int ret = avcodec_send_packet(ctx, p.pkts[i]);
    if(ret == AVERROR(EAGAIN))
      return i;
    REQUIRE(ret == 0);
  }
  FAIL("codec accepted every packet; backpressure was never created");
  return SIZE_MAX;
}
}

TEST_CASE(
    "a packet refused with EAGAIN is retried after draining instead of dropped",
    "[video][decoder]")
{
  Packets p;
  AVCodecContext* enc{};
  encode({.codec = AV_CODEC_ID_MPEG1VIDEO, .frames = 12, .gop_size = 1}, p, &enc);

  TestDecoder t{enc, /* ignorePts: */ true};

  const size_t refused = preload_until_backpressure(t.dec.m_codecContext, p);
  REQUIRE(refused >= 2);

  std::vector<int64_t> pts;
  for(size_t i = refused; i < p.pkts.size(); i++)
    t.collect(t.dec.enqueue_frame(p.pkts[i]), pts);
  t.collect(t.dec.enqueue_frame(nullptr), pts);

  REQUIRE(t.dec.m_finished);

  // Every input frame came out, exactly once, in order — including the two
  // preloaded ones and the one whose packet was initially refused.
  REQUIRE(pts.size() == 12);
  for(int64_t i = 0; i < 12; i++)
    REQUIRE(pts[i] == i);

  avcodec_free_context(&enc);
}

TEST_CASE(
    "a frame discarded for its negative pts does not cost the refused packet",
    "[video][decoder]")
{
  Packets p;
  AVCodecContext* enc{};
  encode({.codec = AV_CODEC_ID_MPEG1VIDEO, .frames = 12, .gop_size = 1}, p, &enc);

  // Shift the timeline so the first two frames land at pts -2 and -1: the
  // decode policy discards them (ignorePTS = false). They are exactly the
  // frames pending inside the codec when backpressure hits, so the drain
  // that frees the codec yields only discarded frames — which is still
  // progress, and the refused packet must still be retried.
  for(auto* pkt : p.pkts)
  {
    pkt->pts -= 2;
    pkt->dts -= 2;
  }

  TestDecoder t{enc, /* ignorePts: */ false};

  const size_t refused = preload_until_backpressure(t.dec.m_codecContext, p);
  REQUIRE(refused >= 2);

  std::vector<int64_t> pts;
  for(size_t i = refused; i < p.pkts.size(); i++)
    t.collect(t.dec.enqueue_frame(p.pkts[i]), pts);
  t.collect(t.dec.enqueue_frame(nullptr), pts);

  // 12 in, 2 discarded by the pts policy, 10 out — and pts 0 is present:
  // losing the refused packet as well would start the run at 1.
  REQUIRE(pts.size() == 10);
  for(int64_t i = 0; i < 10; i++)
    REQUIRE(pts[i] == i);

  avcodec_free_context(&enc);
}

TEST_CASE(
    "every frame still inside a delayed decoder comes out of the flush",
    "[video][decoder]")
{
  // B-frames plus libavcodec's automatic frame threading give the decoder a
  // delay of many frames: when the last packet has been sent, a large part
  // of the clip only exists inside the codec. The single
  // enqueue_frame(nullptr) at EOF has to drain all of it, in order.
  Packets p;
  AVCodecContext* enc{};
  encode(
      {.codec = AV_CODEC_ID_MPEG4, .frames = 32, .gop_size = 8, .max_b_frames = 2},
      p, &enc);

  TestDecoder t{enc, /* ignorePts: */ true, /* threads: */ 0};

  std::vector<int64_t> pts;
  for(auto* pkt : p.pkts)
    t.collect(t.dec.enqueue_frame(pkt), pts);

  const size_t before_flush = pts.size();
  t.collect(t.dec.enqueue_frame(nullptr), pts);

  // The decoder was actually holding frames back — otherwise this test
  // exercises nothing.
  REQUIRE(before_flush < 32);
  REQUIRE(pts.size() == 32);
  for(int64_t i = 0; i < 32; i++)
    REQUIRE(pts[i] == i);

  avcodec_free_context(&enc);
}

TEST_CASE("flushing after the flush is a harmless no-op", "[video][decoder]")
{
  Packets p;
  AVCodecContext* enc{};
  encode({.codec = AV_CODEC_ID_MPEG1VIDEO, .frames = 3, .gop_size = 1}, p, &enc);

  TestDecoder t{enc, /* ignorePts: */ true};

  std::vector<int64_t> pts;
  for(auto* pkt : p.pkts)
    t.collect(t.dec.enqueue_frame(pkt), pts);
  t.collect(t.dec.enqueue_frame(nullptr), pts);
  REQUIRE(pts.size() == 3);
  REQUIRE(t.dec.m_finished);

  auto again = t.dec.enqueue_frame(nullptr);
  REQUIRE(again.frame == nullptr);
  REQUIRE(t.dec.m_finished);

  avcodec_free_context(&enc);
}

TEST_CASE(
    "a B-frame mp4 comes out whole and in presentation order through "
    "VideoDecoder",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  constexpr int N = 50;

  // Encode an mp4 whose codec reorders (mpeg4, gop 10, 2 B-frames): the last
  // frames of the clip only exist in the decoder's reorder buffer and reach
  // the queue through the EOF flush path.
  const auto path
      = (uniqueTempPath("score_videodecoder_test") += ".mp4").string();

  {
    Packets p;
    AVCodecContext* enc{};
    encode(
        {.codec = AV_CODEC_ID_MPEG4, .frames = N, .gop_size = 10, .max_b_frames = 2,
         .global_header = true},
        p, &enc);

    AVFormatContext* mux{};
    REQUIRE(
        avformat_alloc_output_context2(&mux, nullptr, "mp4", path.c_str()) >= 0);
    AVStream* st = avformat_new_stream(mux, nullptr);
    REQUIRE(st);
    REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
    st->time_base = enc->time_base;

    REQUIRE(avio_open(&mux->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0);
    REQUIRE(avformat_write_header(mux, nullptr) >= 0);
    for(auto* pkt : p.pkts)
    {
      av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
      pkt->stream_index = st->index;
      REQUIRE(av_interleaved_write_frame(mux, pkt) >= 0);
    }
    REQUIRE(av_write_trailer(mux) >= 0);
    avio_closep(&mux->pb);
    avformat_free_context(mux);
    avcodec_free_context(&enc);
  }

  // Diagnostic tiers: demux the file back and count packets, then decode
  // them through the enqueue_frame path directly, so a failure localizes
  // itself between the file, the decode core and the VideoDecoder thread.
  {
    AVFormatContext* demux{};
    REQUIRE(avformat_open_input(&demux, path.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(demux, nullptr) >= 0);
    AVPacket* pkt = av_packet_alloc();
    int npackets = 0;
    while(av_read_frame(demux, pkt) >= 0)
    {
      if(pkt->stream_index == 0)
        npackets++;
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avformat_close_input(&demux);
    REQUIRE(npackets == N);
  }
  {
    AVFormatContext* demux{};
    REQUIRE(avformat_open_input(&demux, path.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(demux, nullptr) >= 0);
    AVCodecParameters* par = demux->streams[0]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    REQUIRE(codec);
    AVCodecContext* dctx = avcodec_alloc_context3(codec);
    REQUIRE(avcodec_parameters_to_context(dctx, par) >= 0);
    dctx->pkt_timebase = demux->streams[0]->time_base;
    REQUIRE(avcodec_open2(dctx, codec, nullptr) == 0);

    Video::LibAVDecoder dec;
    dec.m_conf.useAVCodec = true;
    dec.m_conf.ignorePTS = false;
    dec.m_codecContext = dctx;
    dec.m_avstream = demux->streams[0];

    std::vector<int64_t> pts;
    auto collect = [&](Video::ReadFrame r) {
      while(AVFrame* f = dec.m_frames.dequeue_one())
      {
        pts.push_back(f->pts);
        av_frame_free(&f);
      }
      if(r.frame)
      {
        pts.push_back(r.frame->pts);
        av_frame_free(&r.frame);
      }
    };

    AVPacket* pkt = av_packet_alloc();
    while(av_read_frame(demux, pkt) >= 0)
    {
      if(pkt->stream_index == 0)
        collect(dec.enqueue_frame(pkt));
      av_packet_unref(pkt);
    }
    collect(dec.enqueue_frame(nullptr));
    av_packet_free(&pkt);

    if(int(pts.size()) != N)
    {
      std::string got;
      for(auto p : pts)
        got += std::to_string(p) + " ";
      UNSCOPED_INFO("direct decode pts: " << got);
    }
    REQUIRE(int(pts.size()) == N);

    avcodec_free_context(&dctx);
    dec.m_codecContext = nullptr;
    dec.m_avstream = nullptr;
    dec.m_frames.drain();
    avformat_close_input(&demux);
  }

  {
    Video::VideoDecoder dec{Video::DecoderConfiguration{}};
    REQUIRE(dec.load(path));
    // A keyframe every 10 frames: the classification the renderer choice
    // reads, made before the buffering thread consumed any packet.
    CHECK(dec.frame_access == Video::FrameAccess::ShortGop);
    CHECK(dec.max_keyframe_gap == 9);

    using namespace std::chrono;
    using namespace std::chrono_literals;
    std::vector<int64_t> pts;
    const auto deadline = steady_clock::now() + 30s;
    int idle_after_finish = 0;
    while(steady_clock::now() < deadline)
    {
      if(AVFrame* f = dec.dequeue_frame())
      {
        pts.push_back(f->pts);
        dec.release_frame(f);
        idle_after_finish = 0;
      }
      else
      {
        if(dec.m_finished && ++idle_after_finish > 100)
          break;
        std::this_thread::sleep_for(1ms);
      }
    }

    // All 50 frames decoded — nothing dropped mid-stream, nothing left in the
    // reorder buffer at EOF — and in strictly increasing presentation order.
    if(int(pts.size()) != N)
    {
      std::string got;
      for(auto p : pts)
        got += std::to_string(p) + " ";
      UNSCOPED_INFO("decoded pts: " << got);
    }
    REQUIRE(pts.size() == N);
    for(size_t i = 1; i < pts.size(); i++)
      REQUIRE(pts[i] > pts[i - 1]);
  }

  std::error_code ec;
  fs::remove(path, ec);
}

TEST_CASE(
    "a raw elementary stream plays every frame, numbered at its frame rate",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  constexpr int N = 30;
  // Raw H.264 has no container and so no timestamps. Frames without one are
  // numbered from the previous one, one frame at the stream's rate, instead
  // of being dropped as negative.
  const auto path = (uniqueTempPath("score_videodecoder_raw") += ".264").string();
  if(!avcodec_find_encoder(AV_CODEC_ID_H264))
    SKIP("no H.264 encoder in this libavcodec");
  {
    Packets p;
    AVCodecContext* enc{};
    encode({.codec = AV_CODEC_ID_H264, .frames = N, .gop_size = 1}, p, &enc);
    FILE* out = std::fopen(path.c_str(), "wb");
    REQUIRE(out);
    for(auto* pkt : p.pkts)
      std::fwrite(pkt->data, 1, pkt->size, out);
    std::fclose(out);
    avcodec_free_context(&enc);
  }

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  REQUIRE(dec.load(path));
  CHECK(dec.frame_access == Video::FrameAccess::Sequential);

  using namespace std::chrono;
  using namespace std::chrono_literals;
  std::vector<int64_t> pts;
  const auto deadline = steady_clock::now() + 30s;
  int idle_after_finish = 0;
  while(steady_clock::now() < deadline)
  {
    if(AVFrame* f = dec.dequeue_frame())
    {
      pts.push_back(f->pts);
      dec.release_frame(f);
      idle_after_finish = 0;
    }
    else
    {
      if(dec.m_finished && ++idle_after_finish > 100)
        break;
      std::this_thread::sleep_for(1ms);
    }
  }

  REQUIRE(pts.size() == N);
  const int64_t step = pts[1] - pts[0];
  CHECK(pts[0] == 0);
  CHECK(step > 0);
  for(size_t i = 1; i < pts.size(); i++)
    CHECK(pts[i] - pts[i - 1] == step);

  std::error_code ec;
  fs::remove(path, ec);
}

TEST_CASE(
    "closing an image sequence, whose demuxer does its own I/O, does not crash",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  // image2 is AVFMT_NOFILE: it opens each picture itself and leaves the
  // format context's pb null, which every teardown must cope with.
  const auto dir = uniqueTempPath("score_videodecoder_sequence");
  fs::create_directories(dir);

  {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    REQUIRE(codec);
    AVCodecContext* enc = avcodec_alloc_context3(codec);
    enc->width = W;
    enc->height = H;
    enc->pix_fmt = AV_PIX_FMT_RGB24;
    enc->time_base = {1, 25};
    REQUIRE(avcodec_open2(enc, codec, nullptr) == 0);

    AVFrame* frame = av_frame_alloc();
    frame->format = enc->pix_fmt;
    frame->width = W;
    frame->height = H;
    REQUIRE(av_frame_get_buffer(frame, 0) == 0);
    AVPacket* pkt = av_packet_alloc();
    for(int i = 0; i < 3; i++)
    {
      for(int y = 0; y < H; y++)
        std::fill_n(frame->data[0] + y * frame->linesize[0], W * 3, uint8_t(i * 80));
      frame->pts = i;
      REQUIRE(avcodec_send_frame(enc, frame) == 0);
      REQUIRE(avcodec_receive_packet(enc, pkt) == 0);
      char name[32];
      std::snprintf(name, sizeof(name), "img%03d.png", i);
      FILE* f = std::fopen((dir / name).string().c_str(), "wb");
      REQUIRE(f);
      std::fwrite(pkt->data, 1, pkt->size, f);
      std::fclose(f);
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&enc);
  }

  const auto pattern = (dir / "img%03d.png").string();
  {
    AVFormatContext* probe{};
    REQUIRE(avformat_open_input(&probe, pattern.c_str(), nullptr, nullptr) == 0);
    CHECK(probe->pb == nullptr);
    avformat_close_input(&probe);
  }

  SECTION("seeked, as the thumbnailer does")
  {
    AVFormatContext* fmt{};
    REQUIRE(avformat_open_input(&fmt, pattern.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(fmt, nullptr) >= 0);
    CHECK(ossia::seek_to_flick(
        fmt, nullptr, fmt->streams[0], 0, AVSEEK_FLAG_BACKWARD | ossia::OSSIA_LIBAV_SEEK_ROUGH));
    avformat_close_input(&fmt);
  }
  SECTION("opened only, as Gfx::Video::Model::setPath does")
  {
    Video::VideoDecoder dec{Video::DecoderConfiguration{}};
    CHECK(dec.open(pattern));
    CHECK(dec.frame_access == Video::FrameAccess::EveryFrame);
  }
  SECTION("loaded, with its buffering thread running")
  {
    Video::VideoDecoder dec{Video::DecoderConfiguration{}};
    CHECK(dec.load(pattern));
  }

  std::error_code ec;
  fs::remove_all(dir, ec);
}

TEST_CASE(
    "a seek between two distant keyframes comes out at its target, not at the "
    "keyframe before it",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  constexpr int N = 100;

  // One keyframe for the whole clip, with B-frames: every seek lands on the
  // first frame, and the frames from there to the target are decoded, some
  // of them several per packet.
  const auto path = (uniqueTempPath("score_videodecoder_seek") += ".mp4").string();
  {
    Packets p;
    AVCodecContext* enc{};
    encode(
        {.codec = AV_CODEC_ID_MPEG4, .frames = N, .gop_size = 1000, .max_b_frames = 2,
         .global_header = true},
        p, &enc);

    AVFormatContext* mux{};
    REQUIRE(avformat_alloc_output_context2(&mux, nullptr, "mp4", path.c_str()) >= 0);
    AVStream* st = avformat_new_stream(mux, nullptr);
    REQUIRE(st);
    REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
    st->time_base = enc->time_base;
    REQUIRE(avio_open(&mux->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0);
    REQUIRE(avformat_write_header(mux, nullptr) >= 0);
    for(auto* pkt : p.pkts)
    {
      av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
      pkt->stream_index = st->index;
      REQUIRE(av_interleaved_write_frame(mux, pkt) >= 0);
    }
    REQUIRE(av_write_trailer(mux) >= 0);
    avio_closep(&mux->pb);
    avformat_free_context(mux);
    avcodec_free_context(&enc);
  }

  using namespace std::chrono;
  using namespace std::chrono_literals;

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  REQUIRE(dec.load(path));
  CHECK(dec.frame_access == Video::FrameAccess::LongGop);

  // The clip's timestamps, played from its start.
  std::vector<int64_t> all;
  {
    const auto deadline = steady_clock::now() + 30s;
    int idle_after_finish = 0;
    while(steady_clock::now() < deadline)
    {
      if(AVFrame* f = dec.dequeue_frame())
      {
        all.push_back(f->pts);
        dec.release_frame(f);
        idle_after_finish = 0;
      }
      else
      {
        if(dec.m_finished && ++idle_after_finish > 100)
          break;
        std::this_thread::sleep_for(1ms);
      }
    }
  }
  REQUIRE(all.size() == N);

  // The frames the decoder gives after a seek to `flicks`, the ones queued
  // before it left out.
  auto framesAfterSeek = [&](int64_t flicks) {
    const int before = dec.seek_generation.load();
    dec.seek(flicks);
    const auto deadline = steady_clock::now() + 30s;
    while(dec.seek_generation.load() == before && steady_clock::now() < deadline)
      std::this_thread::sleep_for(1ms);
    REQUIRE(dec.seek_generation.load() != before);
    const int generation = dec.seek_generation.load();

    std::vector<int64_t> pts;
    int idle = 0;
    while(steady_clock::now() < deadline && idle < 200)
    {
      if(AVFrame* f = dec.dequeue_frame())
      {
        if(Video::frameGeneration(*f) >= generation)
          pts.push_back(f->pts);
        dec.release_frame(f);
        idle = 0;
      }
      else
      {
        idle++;
        std::this_thread::sleep_for(1ms);
      }
    }
    return pts;
  };
  auto timeOf = [&](int64_t pts) {
    return Video::ptsToFlicks(pts, dec.time_base, dec.start_pts);
  };

  SECTION("mid-clip, halfway through a frame")
  {
    const int64_t frame = timeOf(all[63]) - timeOf(all[62]);
    const auto pts = framesAfterSeek(timeOf(all[62]) + frame / 2);
    REQUIRE(!pts.empty());
    CHECK(pts.front() == all[62]);
    CHECK(pts == std::vector<int64_t>(all.begin() + 62, all.end()));
  }

  SECTION("past the end: the last frame")
  {
    // Back at the start first: a target just after the last frame taken is
    // reached by the frames queued, without a seek.
    const int before = dec.seek_generation.load();
    dec.seek(0);
    const auto deadline = steady_clock::now() + 30s;
    bool taken = false;
    while(!taken && steady_clock::now() < deadline)
    {
      if(AVFrame* f = dec.dequeue_frame())
      {
        taken = Video::frameGeneration(*f) > before;
        dec.release_frame(f);
      }
      else
        std::this_thread::sleep_for(1ms);
    }
    REQUIRE(taken);

    const auto pts = framesAfterSeek(timeOf(all.back()) + 10 * timeOf(all[1]));
    REQUIRE(!pts.empty());
    CHECK(pts.front() == all.back());
  }

  std::error_code ec;
  fs::remove(path, ec);
}

TEST_CASE(
    "a seek between the end of a frame's duration and the next frame shows that "
    "frame",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  constexpr int N = 60;

  // 30 frames per second in matroska's millisecond time base: the frames are
  // 33 or 34 ms apart and each lasts 33 ms, so a time can fall after the end
  // of a frame's duration and before the next frame. The frame shown there is
  // the last one starting before it.
  const auto path = (uniqueTempPath("score_videodecoder_gap") += ".mkv").string();
  {
    Packets p;
    AVCodecContext* enc{};
    encode(
        {.codec = AV_CODEC_ID_MPEG4, .frames = N, .gop_size = 1000, .max_b_frames = 2,
         .global_header = true, .rate = 30},
        p, &enc);

    AVFormatContext* mux{};
    REQUIRE(avformat_alloc_output_context2(&mux, nullptr, "matroska", path.c_str()) >= 0);
    AVStream* st = avformat_new_stream(mux, nullptr);
    REQUIRE(st);
    REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
    st->time_base = enc->time_base;
    REQUIRE(avio_open(&mux->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0);
    REQUIRE(avformat_write_header(mux, nullptr) >= 0);
    for(auto* pkt : p.pkts)
    {
      av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
      pkt->stream_index = st->index;
      REQUIRE(av_interleaved_write_frame(mux, pkt) >= 0);
    }
    REQUIRE(av_write_trailer(mux) >= 0);
    avio_closep(&mux->pb);
    avformat_free_context(mux);
    avcodec_free_context(&enc);
  }

  using namespace std::chrono;
  using namespace std::chrono_literals;

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  REQUIRE(dec.load(path));

  std::vector<int64_t> all;
  std::vector<int64_t> durations;
  {
    const auto deadline = steady_clock::now() + 30s;
    int idle_after_finish = 0;
    while(steady_clock::now() < deadline)
    {
      if(AVFrame* f = dec.dequeue_frame())
      {
        all.push_back(f->pts);
#if(LIBAVUTIL_VERSION_MAJOR < 58)
        durations.push_back(f->pkt_duration);
#else
        durations.push_back(f->duration);
#endif
        dec.release_frame(f);
        idle_after_finish = 0;
      }
      else
      {
        if(dec.m_finished && ++idle_after_finish > 100)
          break;
        std::this_thread::sleep_for(1ms);
      }
    }
  }
  REQUIRE(all.size() == N);

  // A frame whose duration ends before the next frame starts.
  size_t gap = 0;
  for(size_t i = N / 2; i + 1 < all.size(); i++)
  {
    if(durations[i] > 0 && all[i] + durations[i] < all[i + 1])
    {
      gap = i;
      break;
    }
  }
  REQUIRE(gap > 0);

  auto timeOf = [&](int64_t pts) {
    return Video::ptsToFlicks(pts, dec.time_base, dec.start_pts);
  };
  // Half a tick before the next frame.
  const int64_t target
      = (timeOf(all[gap] + durations[gap]) + timeOf(all[gap + 1])) / 2;
  REQUIRE(target > timeOf(all[gap] + durations[gap]));
  REQUIRE(target < timeOf(all[gap + 1]));

  const int before = dec.seek_generation.load();
  dec.seek(target);
  const auto deadline = steady_clock::now() + 30s;
  while(dec.seek_generation.load() == before && steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  REQUIRE(dec.seek_generation.load() != before);
  const int generation = dec.seek_generation.load();

  std::vector<int64_t> pts;
  int idle = 0;
  while(steady_clock::now() < deadline && idle < 200)
  {
    if(AVFrame* f = dec.dequeue_frame())
    {
      if(Video::frameGeneration(*f) >= generation)
        pts.push_back(f->pts);
      dec.release_frame(f);
      idle = 0;
    }
    else
    {
      idle++;
      std::this_thread::sleep_for(1ms);
    }
  }
  REQUIRE(!pts.empty());
  CHECK(pts.front() == all[gap]);
  CHECK(pts == std::vector<int64_t>(all.begin() + gap, all.end()));

  std::error_code ec;
  fs::remove(path, ec);
}

// A file that ends in garbage instead of at a frame boundary: the demuxer
// returns AVERROR_INVALIDDATA there, for ever, never AVERROR_EOF (y4m reads a
// frame header that is not one). A seek into the last frame holds that frame
// back until a later one or the end shows it is the one at the target; the
// end here is that error, and the frame must still be shown, once.
TEST_CASE(
    "a seek into the last frame of a file ending in garbage shows that frame",
    "[video][decoder]")
{
  namespace fs = std::filesystem;
  constexpr int N = 30;
  const auto path = (uniqueTempPath("score_videodecoder_garbage_tail") += ".y4m").string();
  {
    FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f);
    std::fprintf(f, "YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C420jpeg\n", W, H);
    std::vector<unsigned char> picture(W * H * 3 / 2);
    for(int i = 0; i < N; i++)
    {
      std::fill(picture.begin(), picture.end(), (unsigned char)(i * 8));
      std::fputs("FRAME\n", f);
      std::fwrite(picture.data(), 1, picture.size(), f);
    }
    // Not a frame header: the demuxer stops on it.
    std::vector<unsigned char> garbage(3000);
    for(std::size_t i = 0; i < garbage.size(); i++)
      garbage[i] = (unsigned char)(i * 37 + 11);
    std::fwrite(garbage.data(), 1, garbage.size(), f);
    std::fclose(f);
  }

  // The tail is what this test is about: the demuxer must end on an error
  // that is not AVERROR_EOF.
  {
    AVFormatContext* fmt{};
    REQUIRE(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(fmt, nullptr) >= 0);
    AVPacket* pkt = av_packet_alloc();
    int packets = 0, end = 0;
    while((end = av_read_frame(fmt, pkt)) >= 0)
    {
      packets++;
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avformat_close_input(&fmt);
    REQUIRE(packets == N);
    REQUIRE(end != AVERROR_EOF);
    REQUIRE(end != AVERROR(EAGAIN));
  }

  using namespace std::chrono;
  using namespace std::chrono_literals;

  Video::VideoDecoder dec{Video::DecoderConfiguration{}};
  REQUIRE(dec.load(path));

  // Half a frame into the last one: no frame starts after it.
  const int64_t last = dec.start_pts + (N - 1);
  const int64_t target
      = (Video::ptsToFlicks(last, dec.time_base, dec.start_pts)
         + Video::ptsToFlicks(last + 1, dec.time_base, dec.start_pts))
        / 2;
  const int before = dec.seeks_done.load();
  dec.seek(target);
  const auto deadline = steady_clock::now() + 30s;
  while(dec.seeks_done.load() == before && steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);
  REQUIRE(dec.seeks_done.load() == before + 1);
  const int generation = dec.seek_generation.load();

  std::vector<int64_t> pts;
  int idle = 0;
  while(steady_clock::now() < deadline && idle < 200)
  {
    if(AVFrame* f = dec.dequeue_frame())
    {
      if(Video::frameGeneration(*f) >= generation)
        pts.push_back(f->pts);
      dec.release_frame(f);
      idle = 0;
    }
    else
    {
      idle++;
      std::this_thread::sleep_for(1ms);
    }
  }
  // Shown once: a reader asking for the same seek again would mean the seek
  // landed on nothing.
  CHECK(pts == std::vector<int64_t>{last});
  CHECK(dec.seeks_done.load() == before + 1);

  std::error_code ec;
  fs::remove(path, ec);
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("video decoder tests need libav", "[video][decoder]")
{
  SUCCEED("score was built without libav; nothing to test");
}

#endif
