// score::gfx::VideoFrameReader::nextFrame(): which queued frame the frame-queue
// video renderer shows at a time. The frame shown is the last one starting at
// or before the time, as the decoder's seek lands on it.
//
// In a rounded time base, 30 fps in matroska's milliseconds, frames are 33 or
// 34 ms apart and each lasts 33 ms: a time can fall after the end of a frame's
// duration and before the next frame starts. The frame there is the earlier
// one; showing none of them keeps the frame from before a seek on screen, and
// a reader going backwards then seeks again and again for that time.
//
// VideoFrameReader::readNextFrame() going backwards asks the decoder for one
// seek at a time, the next once the decoder has taken the last. A request can
// be replaced before the decoder takes it, by a transport to about where the
// video already is, which the decoder takes without seeking: the reader must
// still see it taken, or it never seeks backwards again.

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV

#include <Gfx/Graph/VideoNode.hpp>
#include <Video/FrameQueue.hpp>
#include <Video/VideoDecoder.hpp>
#include <Video/VideoInterface.hpp>

#include <ossia/detail/flicks.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
//! Frames given in order, all of the current seek generation.
struct QueuedFrames final : Video::VideoInterface
{
  std::deque<AVFrame*> queue;
  std::vector<AVFrame*> released;

  QueuedFrames()
  {
    time_base = {1, 1000};
    fps = 30.;
    flicks_per_dts = ossia::flicks_per_second<double> / 1000.;
    dts_per_flicks = 1. / flicks_per_dts;
    start_pts = 0;
  }

  ~QueuedFrames() override
  {
    for(auto* f : queue)
      av_frame_free(&f);
    for(auto* f : released)
      av_frame_free(&f);
  }

  void push(int64_t pts, int64_t duration)
  {
    AVFrame* f = av_frame_alloc();
    f->pts = pts;
#if(LIBAVUTIL_VERSION_MAJOR < 58)
    f->pkt_duration = duration;
#else
    f->duration = duration;
#endif
    queue.push_back(f);
  }

  AVFrame* dequeue_frame() noexcept override
  {
    if(queue.empty())
      return nullptr;
    auto f = queue.front();
    queue.pop_front();
    return f;
  }

  void release_frame(AVFrame* f) noexcept override { released.push_back(f); }
};

double seconds(int64_t ms)
{
  return ms / 1000.;
}

//! A 64x48 MPEG-4 clip of `frames` frames at 25 fps in mp4, a keyframe every
//! `gop` frames.
std::string writeClip(int frames, int gop)
{
  static std::atomic_int counter{};
  const auto path
      = (std::filesystem::temp_directory_path()
         / ("score_videoreader_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "_" + std::to_string(counter++) + ".mp4"))
            .string();

  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  REQUIRE(codec);
  AVCodecContext* enc = avcodec_alloc_context3(codec);
  enc->width = 64;
  enc->height = 48;
  enc->pix_fmt = AV_PIX_FMT_YUV420P;
  enc->time_base = {1, 25};
  enc->framerate = {25, 1};
  enc->gop_size = gop;
  enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  REQUIRE(avcodec_open2(enc, codec, nullptr) == 0);

  AVFormatContext* mux{};
  REQUIRE(avformat_alloc_output_context2(&mux, nullptr, "mp4", path.c_str()) >= 0);
  AVStream* st = avformat_new_stream(mux, nullptr);
  REQUIRE(st);
  REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
  st->time_base = enc->time_base;
  REQUIRE(avio_open(&mux->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0);
  REQUIRE(avformat_write_header(mux, nullptr) >= 0);

  AVFrame* frame = av_frame_alloc();
  frame->format = enc->pix_fmt;
  frame->width = enc->width;
  frame->height = enc->height;
  REQUIRE(av_frame_get_buffer(frame, 0) == 0);
  AVPacket* pkt = av_packet_alloc();
  auto pump = [&](AVFrame* f) {
    REQUIRE(avcodec_send_frame(enc, f) == 0);
    while(avcodec_receive_packet(enc, pkt) == 0)
    {
      pkt->duration = 1;
      av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
      pkt->stream_index = st->index;
      REQUIRE(av_interleaved_write_frame(mux, pkt) >= 0);
    }
  };
  for(int i = 0; i < frames; i++)
  {
    REQUIRE(av_frame_make_writable(frame) == 0);
    for(int p = 0; p < 3; p++)
      for(int y = 0; y < (p ? 24 : 48); y++)
        for(int x = 0; x < (p ? 32 : 64); x++)
          frame->data[p][y * frame->linesize[p] + x] = uint8_t(x + y + i * 3 + p * 50);
    frame->pts = i;
    pump(frame);
  }
  pump(nullptr);
  REQUIRE(av_write_trailer(mux) >= 0);
  avio_closep(&mux->pb);
  avformat_free_context(mux);
  av_packet_free(&pkt);
  av_frame_free(&frame);
  avcodec_free_context(&enc);
  return path;
}
}

TEST_CASE(
    "a time between the end of a frame's duration and the next frame shows that "
    "frame",
    "[video][gfx]")
{
  auto dec = std::make_shared<QueuedFrames>();
  // 1000 ms -> 1033 ms (33 ms) -> 1067 ms: 1066.2 ms is after the end of the
  // second frame and before the third.
  dec->push(1000, 33);
  dec->push(1033, 33);
  dec->push(1067, 33);
  dec->push(1100, 33);

  score::gfx::VideoNode node{dec, std::nullopt};
  std::vector<AVFrame*> toFree;
  AVFrame* next{};

  SECTION("among the frames dequeued")
  {
    node.standardUBO.time = seconds(1066) + 0.0002;
    AVFrame* shown
        = score::gfx::VideoFrameReader::nextFrame(node, *dec, toFree, next, true);
    REQUIRE(shown);
    CHECK(shown->pts == 1033);
    REQUIRE(next);
    CHECK(next->pts == 1067);
    REQUIRE(toFree.size() == 1);
    CHECK(toFree[0]->pts == 1000);
    av_frame_free(&shown);
  }

  SECTION("the frame held back from the previous tick")
  {
    node.standardUBO.time = seconds(1020);
    AVFrame* shown
        = score::gfx::VideoFrameReader::nextFrame(node, *dec, toFree, next, true);
    REQUIRE(shown);
    CHECK(shown->pts == 1000);
    av_frame_free(&shown);

    // The next frame has not started: held back.
    node.standardUBO.time = seconds(1025);
    shown = score::gfx::VideoFrameReader::nextFrame(node, *dec, toFree, next, true);
    CHECK(shown == nullptr);
    REQUIRE(next);
    CHECK(next->pts == 1033);

    // Past its duration, before the frame after it.
    node.standardUBO.time = seconds(1066) + 0.0002;
    shown = score::gfx::VideoFrameReader::nextFrame(node, *dec, toFree, next, true);
    REQUIRE(shown);
    CHECK(shown->pts == 1033);
    av_frame_free(&shown);
    REQUIRE(next);
    CHECK(next->pts == 1067);

    node.standardUBO.time = seconds(1100) + 0.0005;
    shown = score::gfx::VideoFrameReader::nextFrame(node, *dec, toFree, next, true);
    REQUIRE(shown);
    CHECK(shown->pts == 1100);
    av_frame_free(&shown);
  }

  for(auto* f : toFree)
    av_frame_free(&f);
  if(next)
    av_frame_free(&next);
}

TEST_CASE(
    "a backward seek replaced by a transport to where the video is does not stop "
    "the reader from seeking backwards",
    "[video][gfx]")
{
  using namespace std::chrono;
  using namespace std::chrono_literals;
  const auto path = writeClip(250, 25);

  auto dec = std::make_shared<Video::VideoDecoder>(Video::DecoderConfiguration{});
  REQUIRE(dec->load(path));
  score::gfx::VideoNode node{dec, std::nullopt};

  // A tick at time t, with the decoder given the time to fill its queue.
  auto tick = [&](double t) {
    node.standardUBO.time = t;
    for(int i = 0; i < 200 && dec->m_frames.size() == 0 && !dec->m_finished; i++)
      std::this_thread::sleep_for(1ms);
    node.reader.readNextFrame(node);
  };
  auto shownTime = [&]() -> double {
    std::lock_guard l{node.reader.m_frameLock};
    auto& cur = node.reader.m_currentFrame;
    if(!cur || !cur->frame)
      return -1.;
    return dec->flicks_per_dts * double(cur->frame->pts - dec->start_pts)
           / ossia::flicks_per_second<double>;
  };

  double t = 0.;
  for(int trial = 0; trial < 5; trial++)
  {
    // Play forwards for a second.
    for(int i = 0; i < 25; i++, t += 0.04)
      tick(t);
    const double shown = shownTime();
    REQUIRE(shown > 0.);

    // Going back a second: the reader asks for a seek. A transport to a bit
    // after the frame on screen replaces it at once, before the decoding
    // thread takes it; the frames already queued reach that time, so the
    // decoder takes it without seeking.
    node.standardUBO.time = shown - 1.;
    node.reader.readNextFrame(node);
    dec->seek(int64_t((shown + 0.06) * ossia::flicks_per_second<double>));
    std::this_thread::sleep_for(100ms);

    // Further back: the reader must seek there, and show it.
    const double back = std::max(0.2, shown - 3.);
    const auto deadline = steady_clock::now() + 5s;
    while(steady_clock::now() < deadline && std::abs(shownTime() - back) > 0.1)
    {
      tick(back);
      std::this_thread::sleep_for(5ms);
    }
    INFO("trial " << trial << ": frame at " << shown << ", went back to " << back);
    REQUIRE(std::abs(shownTime() - back) <= 0.1);
    t = back;
  }

  node.reader.releaseAllFrames();
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("video frame reader tests need libav", "[video][gfx]")
{
  SUCCEED("score was built without libav; nothing to test");
}

#endif
