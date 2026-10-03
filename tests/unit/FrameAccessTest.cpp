// Video::classifyFrameAccess(): what reaching an arbitrary frame of a stream
// costs, read off its keyframe layout.
//
// Clips are encoded in-process with codecs and muxers every libavcodec build
// has (mpeg4, png; mp4, mpegts, matroska), so that each source of the answer
// is exercised: the descriptor's intra-only flag, a complete container index
// (mp4), and the packet probe (mpegts, whose demuxer has no index; matroska,
// whose index lists only some keyframes; image sequences, whose demuxer does
// its own I/O).

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV

#include <Video/FrameAccess.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

using Video::FrameAccess;

namespace
{
namespace fs = std::filesystem;

constexpr int W = 64;
constexpr int H = 48;

AVFrame* testFrame(AVPixelFormat fmt, int i)
{
  AVFrame* frame = av_frame_alloc();
  frame->format = fmt;
  frame->width = W;
  frame->height = H;
  REQUIRE(av_frame_get_buffer(frame, 0) == 0);
  REQUIRE(av_frame_make_writable(frame) == 0);
  for(int p = 0; p < AV_NUM_DATA_POINTERS && frame->data[p]; p++)
  {
    const int rows = (p == 0 || fmt != AV_PIX_FMT_YUV420P) ? H : H / 2;
    for(int y = 0; y < rows; y++)
      for(int x = 0; x < frame->linesize[p]; x++)
        frame->data[p][y * frame->linesize[p] + x] = uint8_t(x * 3 + y * 5 + i * 7);
  }
  frame->pts = i;
  return frame;
}

// Encodes `frames` mpeg4 pictures with a keyframe every `gop` into `path`,
// muxed by `muxer`.
void writeClip(const std::string& path, const char* muxer, int frames, int gop)
{
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  REQUIRE(codec);
  AVCodecContext* enc = avcodec_alloc_context3(codec);
  enc->width = W;
  enc->height = H;
  enc->pix_fmt = AV_PIX_FMT_YUV420P;
  enc->time_base = {1, 25};
  enc->framerate = {25, 1};
  enc->gop_size = gop;
  enc->max_b_frames = 0;

  AVFormatContext* mux{};
  REQUIRE(avformat_alloc_output_context2(&mux, nullptr, muxer, path.c_str()) >= 0);
  if(mux->oformat->flags & AVFMT_GLOBALHEADER)
    enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  REQUIRE(avcodec_open2(enc, codec, nullptr) == 0);

  AVStream* st = avformat_new_stream(mux, nullptr);
  REQUIRE(avcodec_parameters_from_context(st->codecpar, enc) == 0);
  st->time_base = enc->time_base;
  REQUIRE(avio_open(&mux->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0);
  REQUIRE(avformat_write_header(mux, nullptr) >= 0);

  AVPacket* pkt = av_packet_alloc();
  auto drain = [&] {
    while(avcodec_receive_packet(enc, pkt) == 0)
    {
      av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
      pkt->stream_index = st->index;
      REQUIRE(av_interleaved_write_frame(mux, pkt) >= 0);
    }
  };
  for(int i = 0; i < frames; i++)
  {
    AVFrame* frame = testFrame(enc->pix_fmt, i);
    REQUIRE(avcodec_send_frame(enc, frame) == 0);
    av_frame_free(&frame);
    drain();
  }
  avcodec_send_frame(enc, nullptr);
  drain();

  REQUIRE(av_write_trailer(mux) >= 0);
  av_packet_free(&pkt);
  avio_closep(&mux->pb);
  avformat_free_context(mux);
  avcodec_free_context(&enc);
}

// Encodes `frames` H.264 pictures, all keyframes, as a raw Annex B elementary
// stream: no container, so no timestamps. False when libavcodec has no H.264
// encoder.
bool writeRawH264(const std::string& path, int frames)
{
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if(!codec)
    return false;
  AVCodecContext* enc = avcodec_alloc_context3(codec);
  enc->width = W;
  enc->height = H;
  enc->pix_fmt = AV_PIX_FMT_YUV420P;
  enc->time_base = {1, 25};
  enc->framerate = {25, 1};
  enc->gop_size = 1;
  enc->max_b_frames = 0;
  REQUIRE(avcodec_open2(enc, codec, nullptr) == 0);

  FILE* out = std::fopen(path.c_str(), "wb");
  REQUIRE(out);
  AVPacket* pkt = av_packet_alloc();
  auto drain = [&] {
    while(avcodec_receive_packet(enc, pkt) == 0)
    {
      std::fwrite(pkt->data, 1, pkt->size, out);
      av_packet_unref(pkt);
    }
  };
  for(int i = 0; i < frames; i++)
  {
    AVFrame* frame = testFrame(enc->pix_fmt, i);
    REQUIRE(avcodec_send_frame(enc, frame) == 0);
    av_frame_free(&frame);
    drain();
  }
  avcodec_send_frame(enc, nullptr);
  drain();
  std::fclose(out);
  av_packet_free(&pkt);
  avcodec_free_context(&enc);
  return true;
}

// Writes `frames` PNG files img000.png... into `dir`.
void writePngSequence(const fs::path& dir, int frames)
{
  fs::create_directories(dir);
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
  REQUIRE(codec);
  AVCodecContext* enc = avcodec_alloc_context3(codec);
  enc->width = W;
  enc->height = H;
  enc->pix_fmt = AV_PIX_FMT_RGB24;
  enc->time_base = {1, 25};
  REQUIRE(avcodec_open2(enc, codec, nullptr) == 0);

  AVPacket* pkt = av_packet_alloc();
  for(int i = 0; i < frames; i++)
  {
    AVFrame* frame = testFrame(enc->pix_fmt, i);
    REQUIRE(avcodec_send_frame(enc, frame) == 0);
    av_frame_free(&frame);
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
  avcodec_free_context(&enc);
}

struct Opened
{
  AVFormatContext* fmt{};
  AVStream* st{};
  explicit Opened(const std::string& path)
  {
    REQUIRE(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(fmt, nullptr) >= 0);
    const int s = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    REQUIRE(s >= 0);
    st = fmt->streams[s];
  }
  ~Opened() { avformat_close_input(&fmt); }

  //! The next packet of the stream must be its first, a keyframe.
  void requireAtStart()
  {
    AVPacket* pkt = av_packet_alloc();
    bool found = false;
    while(av_read_frame(fmt, pkt) >= 0)
    {
      if(pkt->stream_index == st->index)
      {
        found = true;
        CHECK((pkt->flags & AV_PKT_FLAG_KEY));
        if(st->start_time != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE)
          CHECK(pkt->pts == st->start_time);
        av_packet_unref(pkt);
        break;
      }
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    CHECK(found);
  }
};

struct TempDir
{
  fs::path path;
  explicit TempDir(const char* name)
      : path{fs::temp_directory_path() / name}
  {
    fs::create_directories(path);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
}

TEST_CASE("The keyframe layout classifies a stream", "[video][frameaccess]")
{
  TempDir dir{"score_frame_access_test"};

  struct Row
  {
    const char* muxer;
    const char* ext;
    int gop;
    FrameAccess access;
    int min_gap, max_gap;
  };
  // mp4 lists every frame in its index: the gap is exact. mpegts has no index
  // and matroska's lists only keyframes: the packet probe answers, and stops
  // reading once a run is longer than any short GOP.
  const Row rows[]{
      {"mp4", "mp4", 1, FrameAccess::EveryFrame, 0, 0},
      {"mpegts", "ts", 1, FrameAccess::EveryFrame, 0, 0},
      {"matroska", "mkv", 1, FrameAccess::EveryFrame, 0, 0},
      {"mp4", "mp4", 10, FrameAccess::ShortGop, 9, 9},
      {"mpegts", "ts", 10, FrameAccess::ShortGop, 9, 9},
      {"matroska", "mkv", 10, FrameAccess::ShortGop, 9, 9},
      {"mp4", "mp4", 60, FrameAccess::LongGop, 59, 59},
      {"mpegts", "ts", 60, FrameAccess::LongGop, 16, 59},
      {"matroska", "mkv", 60, FrameAccess::LongGop, 16, 59},
  };

  for(const auto& row : rows)
  {
    const auto path
        = (dir.path / ("gop" + std::to_string(row.gop) + "." + row.ext)).string();
    writeClip(path, row.muxer, 80, row.gop);

    INFO(row.muxer << ", keyframe every " << row.gop);
    Opened f{path};
    const auto probe = Video::classifyFrameAccess(*f.fmt, *f.st);
    CHECK(probe.access == row.access);
    CHECK(probe.max_gap >= row.min_gap);
    CHECK(probe.max_gap <= row.max_gap);
    f.requireAtStart();
  }
}

TEST_CASE("An image sequence is reached frame by frame", "[video][frameaccess]")
{
  // PNG carries no intra-only flag and image2 does its own I/O: the probe
  // reads the packets and rewinds without a pb.
  TempDir dir{"score_frame_access_sequence"};
  writePngSequence(dir.path, 40);

  Opened f{(dir.path / "img%03d.png").string()};
  REQUIRE(f.fmt->pb == nullptr);
  const auto probe = Video::classifyFrameAccess(*f.fmt, *f.st);
  CHECK(probe.access == FrameAccess::EveryFrame);
  CHECK(probe.max_gap == 0);
  f.requireAtStart();
}

TEST_CASE(
    "A raw elementary stream cannot be sought by time", "[video][frameaccess]")
{
  // Raw H.264 carries no timestamps, every frame being a keyframe or not: no
  // time maps to a frame. Probing it reads packets, which a time seek cannot
  // undo -- a failed one leaves the demuxer at its end -- so the probe must
  // still hand the stream back at its first packet.
  TempDir dir{"score_frame_access_raw"};
  const auto path = (dir.path / "raw.264").string();
  if(!writeRawH264(path, 40))
    SKIP("no H.264 encoder in this libavcodec");

  Opened f{path};
  REQUIRE(f.st->start_time == AV_NOPTS_VALUE);
  const auto probe = Video::classifyFrameAccess(*f.fmt, *f.st);
  CHECK(probe.access == FrameAccess::Sequential);

  AVPacket* pkt = av_packet_alloc();
  int packets = 0;
  bool first_is_key = false;
  while(av_read_frame(f.fmt, pkt) >= 0)
  {
    if(pkt->stream_index == f.st->index)
    {
      if(packets == 0)
        first_is_key = pkt->flags & AV_PKT_FLAG_KEY;
      packets++;
    }
    av_packet_unref(pkt);
  }
  av_packet_free(&pkt);
  CHECK(first_is_key);
  CHECK(packets == 40);
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("frame access tests need libav", "[video][frameaccess]") { }

#endif
