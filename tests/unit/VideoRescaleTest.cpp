// Video::Rescale is the CPU fallback every format without a GPU decoder goes
// through: sws_scale from the decoded layout to RGBA. It is handed the stream
// METADATA at open() and a decoded AVFrame at every call, and those two can
// disagree -- a mid-stream resolution change, or a container that lies about
// its size -- so which of the two it believes decides whether sws_scale reads
// inside the source planes or past them.
//
// No file and no codec: the source frames are plain buffers this test owns, so
// what is beyond the frame's own last row is known and can be asserted about.

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/FrameQueue.hpp>
#include <Video/Rescale.hpp>
#include <Video/VideoInterface.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace
{
constexpr int kW = 64;
constexpr int kStorageH = 64;
constexpr int kRedRows = 32;

// A 64x64 RGB24 buffer: the top half red, the bottom half green. A frame that
// declares only the top half must never make the green rows visible.
std::vector<uint8_t> twoToneBuffer()
{
  std::vector<uint8_t> buf(std::size_t(kW) * kStorageH * 3);
  for(int y = 0; y < kStorageH; y++)
  {
    const bool red = y < kRedRows;
    for(int x = 0; x < kW; x++)
    {
      uint8_t* px = buf.data() + (std::size_t(y) * kW + x) * 3;
      px[0] = red ? 255 : 0;
      px[1] = red ? 0 : 255;
      px[2] = 0;
    }
  }
  return buf;
}

// An AVFrame borrowing `buf`: no AVBufferRef, so av_frame_free() leaves the
// bytes to us and the frame can describe fewer rows than the buffer holds.
Video::AVFramePointer borrowFrame(std::vector<uint8_t>& buf, int height)
{
  Video::AVFramePointer f{av_frame_alloc()};
  f->format = AV_PIX_FMT_RGB24;
  f->width = kW;
  f->height = height;
  f->data[0] = buf.data();
  f->linesize[0] = kW * 3;
  return f;
}

struct Rgba
{
  uint8_t r{}, g{}, b{}, a{};
};

Rgba pixelAt(const AVFrame& f, int x, int y)
{
  const uint8_t* px = f.data[0] + std::size_t(y) * f.linesize[0] + std::size_t(x) * 4;
  return {px[0], px[1], px[2], px[3]};
}

bool near(Rgba got, Rgba want, int tol = 12)
{
  return std::abs(int(got.r) - int(want.r)) <= tol
         && std::abs(int(got.g) - int(want.g)) <= tol
         && std::abs(int(got.b) - int(want.b)) <= tol;
}

// One rescale() round trip. Returns the RGBA frame, still owned by the queue.
AVFrame* runRescale(
    Video::Rescale& rescale, Video::FrameQueue& queue, Video::AVFramePointer frame)
{
  Video::ReadFrame read;
  read.frame = frame.get();
  rescale.rescale(queue, frame, read);
  return read.frame;
}
} // namespace

TEST_CASE("the rescaler reads the height the frame declares", "[video][rescale]")
{
  auto buf = twoToneBuffer();

  Video::VideoMetadata meta;
  meta.width = kW;
  meta.height = kStorageH;
  meta.pixel_format = AV_PIX_FMT_RGB24;

  Video::FrameQueue queue;
  Video::Rescale rescale;
  rescale.open(meta);
  REQUIRE(bool(rescale));

  // Control: a frame the size the metadata claims. Both halves are real source
  // rows, so both must come through -- if this reads uniform, the two-tone
  // buffer never reached sws_scale and the case below would prove nothing.
  {
    AVFrame* out = runRescale(rescale, queue, borrowFrame(buf, kStorageH));
    REQUIRE(out != nullptr);
    REQUIRE(out->data[0] != nullptr);
    CHECK(out->width == kW);
    CHECK(out->height == kStorageH);
    CHECK(near(pixelAt(*out, 32, 8), {255, 0, 0, 255}));
    CHECK(near(pixelAt(*out, 32, 56), {0, 255, 0, 255}));
    queue.release(out);
  }

  // The defect: a frame that declares half the metadata height. Row 32 onwards
  // is NOT part of it, so nothing green may appear in the output. Passing the
  // metadata height as sws_scale's srcSliceH read those rows anyway.
  {
    AVFrame* out = runRescale(rescale, queue, borrowFrame(buf, kRedRows));
    REQUIRE(out != nullptr);
    REQUIRE(out->data[0] != nullptr);
    CHECK(out->width == kW);
    CHECK(out->height == kStorageH);
    for(int y : {2, 20, 40, 56, 62})
    {
      INFO("row " << y);
      CHECK(near(pixelAt(*out, 32, y), {255, 0, 0, 255}));
    }
    queue.release(out);
  }
}

#endif
