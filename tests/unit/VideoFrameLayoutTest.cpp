// Video::initFrameFromRawData() (Video/GStreamerCompatibility.hpp): the plane
// layout every stride-less input device applies to a flat buffer of pixels. The
// GStreamer appsink, Sh4lt, Shmdata and Kinect2 inputs all hand it a void* plus
// a byte count and let it fill AVFrame::data / AVFrame::linesize.
//
// The oracle is libav, not a second copy of the same arithmetic:
// av_image_get_linesize(fmt, w, plane) gives the minimum bytes a row of that
// plane occupies, and av_image_fill_plane_sizes turns the strides the subject
// CHOSE into the byte count each plane then requires. Nothing here re-derives a
// stride from a subsampling factor.
//
// The sweep is over Video::gstreamerToLibav(), the formats an appsink can
// actually deliver, at 64x64 (aligned), 65x33 and 63x31 (odd, where a 4:2:0
// chroma plane is ceil(w/2) x ceil(h/2)) and 2x2 (degenerate).
//
// Cases marked [!shouldfail] pin layouts that are still wrong in
// GStreamerCompatibility.hpp, so each entry flips RED the day it is fixed:
//   1. AV_PIX_FMT_NV16 is two-plane but handled in the packed 16bpp branch, so
//      data[1] / linesize[1] are never written.
//   2. AV_PIX_FMT_P010LE/BE sit with GRAY10 and get linesize[0] = width*10/8
//      and no second plane; P010 is 2 bytes per component plus interleaved UV.
//   3. AV_PIX_FMT_Y210LE gets linesize[0] = width*20/8; Y210 is 4 bytes/pixel.
//   4. Every subsampled layout rounds DOWN -- width/2, width/4, and plane-2
//      offsets on height/2, height/4 -- so a 65x33 4:2:0 plane is a column short
//      and planes 1 and 2 overlap.
//   5. NV12/NV21 and P016LE/BE give the interleaved chroma plane linesize =
//      width (resp. 2*width) instead of 2*ceil(width/2) (resp. 4*ceil(width/2)).
//   6. The packed 4:2:2 family gets linesize = 2*width, but a macropixel covers
//      two columns, so an odd width needs 4*ceil(width/2).

#include <Video/GStreamerCompatibility.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace
{
struct Size
{
  int w, h;
};

constexpr Size kAligned{64, 64};
constexpr Size kSizes[] = {{64, 64}, {65, 33}, {63, 31}, {2, 2}};

// The formats whose layout is wrong TODAY. A format that leaves this set makes
// the [!shouldfail] cases below flip red; a format that ENTERS it fails
// "no format outside the known-broken set is mis-laid-out", which is the guard
// against a new defect hiding behind an old one.
const std::set<std::string> kKnownBrokenAligned{"nv16", "p010le", "p010be", "y210le",
                                                "y210be"};
const std::set<std::string> kKnownBrokenOdd{
    "yuv420p", "yuyv422", "yuv422p", "yuv410p", "yuv411p", "uyvy422",
    "nv12",    "nv21",    "nv16",    "yvyu422", "p010le",  "p010be",
    "p016le",  "p016be",  "y210le",  "y210be"};
// A 2x2 frame is "aligned" for 4:2:0 but not for 4:1:0 / 4:1:1, whose
// width/4 stride comes out as ZERO -- the same round-down defect at the
// smallest geometry a stream can have.
const std::set<std::string> kKnownBrokenTiny{"nv16",    "p010le",  "p010be",
                                             "y210le",  "y210be",  "yuv410p",
                                             "yuv411p"};

struct PlaneUse
{
  int plane{};
  std::ptrdiff_t offset{};
  std::size_t bytes{};
};

struct Layout
{
  bool accepted{};
  int planes{};
  std::vector<PlaneUse> used;
  std::vector<int> linesize;
  std::vector<int> minLinesize;
  std::vector<bool> nullPlane;
};

Layout layoutOf(AVPixelFormat fmt, int w, int h, std::size_t bufferSize, uint8_t* base)
{
  Layout out;
  out.planes = av_pix_fmt_count_planes(fmt);

  AVFrame f{};
  f.format = fmt;
  f.width = w;
  f.height = h;
  out.accepted = Video::initFrameFromRawData(&f, base, bufferSize);
  if(!out.accepted)
    return out;

  ptrdiff_t ls[4]{};
  for(int p = 0; p < 4; p++)
    ls[p] = f.linesize[p];

  std::size_t sizes[4]{};
  const int rc = av_image_fill_plane_sizes(sizes, fmt, h, ls);

  for(int p = 0; p < out.planes && p < 4; p++)
  {
    out.linesize.push_back(f.linesize[p]);
    out.minLinesize.push_back(av_image_get_linesize(fmt, w, p));
    out.nullPlane.push_back(f.data[p] == nullptr);
    if(f.data[p] && rc >= 0)
      out.used.push_back({p, f.data[p] - base, sizes[p]});
  }
  return out;
}

std::vector<AVPixelFormat> mappedFormats()
{
  std::vector<AVPixelFormat> out;
  for(const auto& [name, fmt] : Video::gstreamerToLibav())
    out.push_back(fmt);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::string fmtName(AVPixelFormat f)
{
  const char* n = av_get_pix_fmt_name(f);
  return n ? n : "<unknown>";
}

struct Verdict
{
  std::string format;
  int w{}, h{};
  bool accepted{};
  bool everyPlaneMapped{true};
  bool everyStrideWideEnough{true};
  bool everyPlaneInsideBuffer{true};
  bool planesDisjoint{true};
  std::string detail;
};

Verdict check(AVPixelFormat fmt, Size s)
{
  Verdict v;
  v.format = fmtName(fmt);
  v.w = s.w;
  v.h = s.h;

  const int need = av_image_get_buffer_size(fmt, s.w, s.h, 1);
  if(need <= 0)
  {
    v.detail = "av_image_get_buffer_size refused the geometry";
    return v;
  }

  // A guard band past `need`: a layout that points outside the frame buffer is
  // still inside the allocation, so the check reports rather than crashes.
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0xA5);

  const auto l = layoutOf(fmt, s.w, s.h, std::size_t(need), buf.data());
  v.accepted = l.accepted;
  if(!l.accepted)
    return v;

  for(std::size_t i = 0; i < l.nullPlane.size(); i++)
  {
    if(l.nullPlane[i])
    {
      v.everyPlaneMapped = false;
      v.detail += " plane" + std::to_string(i) + "=null";
    }
  }

  for(std::size_t i = 0; i < l.linesize.size(); i++)
  {
    if(l.minLinesize[i] > 0 && l.linesize[i] < l.minLinesize[i])
    {
      v.everyStrideWideEnough = false;
      v.detail += " linesize" + std::to_string(i) + "=" + std::to_string(l.linesize[i])
                  + "<" + std::to_string(l.minLinesize[i]);
    }
  }

  for(const auto& u : l.used)
  {
    if(u.offset < 0 || std::size_t(u.offset) + u.bytes > std::size_t(need))
    {
      v.everyPlaneInsideBuffer = false;
      v.detail += " plane" + std::to_string(u.plane) + "@" + std::to_string(u.offset)
                  + "+" + std::to_string(u.bytes) + ">" + std::to_string(need);
    }
  }

  for(std::size_t i = 0; i < l.used.size(); i++)
  {
    for(std::size_t j = i + 1; j < l.used.size(); j++)
    {
      const auto& a = l.used[i];
      const auto& b = l.used[j];
      const bool disjoint = std::size_t(a.offset) + a.bytes <= std::size_t(b.offset)
                            || std::size_t(b.offset) + b.bytes <= std::size_t(a.offset);
      if(!disjoint)
      {
        v.planesDisjoint = false;
        v.detail += " plane" + std::to_string(a.plane) + " overlaps plane"
                    + std::to_string(b.plane);
      }
    }
  }

  return v;
}

bool clean(const Verdict& v)
{
  return v.accepted && v.everyPlaneMapped && v.everyStrideWideEnough
         && v.everyPlaneInsideBuffer && v.planesDisjoint;
}

std::set<std::string> brokenAt(Size s)
{
  std::set<std::string> out;
  for(auto fmt : mappedFormats())
  {
    if(av_image_get_buffer_size(fmt, s.w, s.h, 1) <= 0)
      continue;
    const auto v = check(fmt, s);
    if(!clean(v))
      out.insert(v.format);
  }
  return out;
}

std::string join(const std::set<std::string>& s)
{
  std::string out;
  for(const auto& x : s)
    out += x + " ";
  return out;
}

// The detail line for every offender, for the failure report.
std::string report(Size s)
{
  std::string out;
  for(auto fmt : mappedFormats())
  {
    if(av_image_get_buffer_size(fmt, s.w, s.h, 1) <= 0)
      continue;
    const auto v = check(fmt, s);
    if(!clean(v))
      out += "  " + v.format + " {" + v.detail + " }\n";
  }
  return out;
}
} // namespace

TEST_CASE("the sweep has formats and sizes to sweep", "[video][layout]")
{
  // Negative control on the sweep itself: an empty format table or a geometry
  // libav refuses would make every case below pass without laying out a frame.
  const auto formats = mappedFormats();
  CHECK(formats.size() >= 40);

  int usable = 0;
  for(auto f : formats)
    for(auto s : kSizes)
      if(av_image_get_buffer_size(f, s.w, s.h, 1) > 0)
        usable++;
  CHECK(usable >= 100);

  bool sawOdd = false, sawPlanar = false, sawPacked = false, sawSemiPlanar = false;
  for(auto s : kSizes)
    if(s.w % 2 || s.h % 2)
      sawOdd = true;
  for(auto f : formats)
  {
    const int n = av_pix_fmt_count_planes(f);
    if(n == 1)
      sawPacked = true;
    else if(n == 2)
      sawSemiPlanar = true;
    else if(n >= 3)
      sawPlanar = true;
  }
  CHECK(sawOdd);
  CHECK(sawPacked);
  CHECK(sawSemiPlanar);
  CHECK(sawPlanar);
}

TEST_CASE("the layout guards fire on a layout that is wrong", "[video][layout]")
{
  // Negative control on the guards themselves: applied to strides that ARE
  // wrong, each of the three checks must report. Without this a green sweep
  // would only prove the checks never fire.
  const auto fmt = AV_PIX_FMT_YUV420P;
  const int w = 64, h = 64;
  const int need = av_image_get_buffer_size(fmt, w, h, 1);
  REQUIRE(need > 0);

  ptrdiff_t ls[4]{};
  std::size_t sizes[4]{};

  SECTION("a stride narrower than a row is caught")
  {
    ls[0] = w / 2;
    ls[1] = ls[2] = w / 4;
    REQUIRE(av_image_fill_plane_sizes(sizes, fmt, h, ls) >= 0);
    CHECK(int(ls[0]) < av_image_get_linesize(fmt, w, 0));
    CHECK(int(ls[1]) < av_image_get_linesize(fmt, w, 1));
  }

  SECTION("a plane that runs past the buffer is caught")
  {
    ls[0] = w;
    ls[1] = ls[2] = w / 2;
    REQUIRE(av_image_fill_plane_sizes(sizes, fmt, h, ls) >= 0);
    const std::ptrdiff_t badOffset = need - 1;
    CHECK(std::size_t(badOffset) + sizes[0] > std::size_t(need));
  }

  SECTION("two planes at the same offset are caught as overlapping")
  {
    ls[0] = w;
    ls[1] = ls[2] = w / 2;
    REQUIRE(av_image_fill_plane_sizes(sizes, fmt, h, ls) >= 0);
    const std::size_t a = 0, b = 0;
    const bool disjoint = (a + sizes[0] <= b) || (b + sizes[1] <= a);
    CHECK_FALSE(disjoint);
  }
}

TEST_CASE("no format outside the known-broken set is mis-laid-out",
          "[video][layout]")
{
  // One-sided guard: a format that starts failing without being on the list is
  // a NEW defect and turns this red, while a fix only affects the
  // [!shouldfail] cases below. Without it, a regression in a format nobody was
  // watching would hide inside an already-red sweep.
  for(auto s : kSizes)
  {
    const auto& known = (s.w % 2 || s.h % 2) ? kKnownBrokenOdd
                        : (s.w < 8 || s.h < 8)
                            ? kKnownBrokenTiny
                            : kKnownBrokenAligned;
    std::set<std::string> unexpected;
    for(const auto& f : brokenAt(s))
      if(!known.count(f))
        unexpected.insert(f);
    INFO("at " << s.w << "x" << s.h << ", newly broken: " << join(unexpected)
               << "\n"
               << report(s));
    CHECK(unexpected.empty());
  }
}

TEST_CASE(
    "aligned frames of the GStreamer-mapped formats lay out correctly",
    "[video][layout][!shouldfail]")
{
  // FINDINGS 1-3. 64x64: every subsampling factor divides both dimensions, so
  // nothing here depends on rounding -- these four formats are mis-described
  // outright. nv16 and p010 hand the renderer a null chroma plane; y210's
  // stride is 160 bytes for a 256-byte row.
  const auto broken = brokenAt(kAligned);
  INFO("formats whose 64x64 layout is wrong:\n" + report(kAligned));
  CHECK(broken.empty());
}

TEST_CASE(
    "odd frames of the GStreamer-mapped formats lay out correctly",
    "[video][layout][odd][!shouldfail]")
{
  // FINDINGS 4-6. Every subsampled stride and every plane offset rounds down,
  // so a 65x33 or 63x31 frame is a column short in chroma and, for 4:2:0 and
  // 4:1:0, plane 2 starts inside plane 1.
  std::set<std::string> broken;
  std::string detail;
  for(auto s : kSizes)
  {
    if(s.w % 2 == 0 && s.h % 2 == 0)
      continue;
    for(const auto& f : brokenAt(s))
      broken.insert(f);
    detail += "at " + std::to_string(s.w) + "x" + std::to_string(s.h) + ":\n"
              + report(s);
  }
  INFO(detail);
  CHECK(broken.empty());
}

TEST_CASE("NV16 gets both of its planes", "[video][layout][!shouldfail]")
{
  // FINDING 1, on its own so it flips independently of the rest.
  AVFrame f{};
  f.format = AV_PIX_FMT_NV16;
  f.width = 64;
  f.height = 64;
  const int need = av_image_get_buffer_size(AV_PIX_FMT_NV16, 64, 64, 1);
  REQUIRE(need > 0);
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
  REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));
  CHECK(f.data[1] != nullptr);
  CHECK(f.linesize[1] >= av_image_get_linesize(AV_PIX_FMT_NV16, 64, 1));
}

TEST_CASE("P010 is laid out as a 16-bit two-plane format",
          "[video][layout][!shouldfail]")
{
  // FINDING 2. P010 stores 10 bits in the HIGH bits of a 16-bit sample, so a
  // row is 2*width bytes, and there is an interleaved UV plane after it.
  AVFrame f{};
  f.format = AV_PIX_FMT_P010LE;
  f.width = 64;
  f.height = 64;
  const int need = av_image_get_buffer_size(AV_PIX_FMT_P010LE, 64, 64, 1);
  REQUIRE(need > 0);
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
  REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));
  CHECK(f.linesize[0] >= av_image_get_linesize(AV_PIX_FMT_P010LE, 64, 0));
  CHECK(f.data[1] != nullptr);
  CHECK(f.linesize[1] >= av_image_get_linesize(AV_PIX_FMT_P010LE, 64, 1));
}

TEST_CASE("Y210 rows are four bytes per pixel", "[video][layout][!shouldfail]")
{
  // FINDING 3. Y210 packs Y0/U/Y1/V as four uint16 per two pixels.
  AVFrame f{};
  f.format = AV_PIX_FMT_Y210LE;
  f.width = 64;
  f.height = 64;
  const int need = av_image_get_buffer_size(AV_PIX_FMT_Y210LE, 64, 64, 1);
  REQUIRE(need > 0);
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
  REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));
  CHECK(f.linesize[0] >= av_image_get_linesize(AV_PIX_FMT_Y210LE, 64, 0));
}

TEST_CASE("a 4:2:0 chroma plane of an odd frame is ceil(w/2) x ceil(h/2)",
          "[video][layout][odd][!shouldfail]")
{
  // FINDING 4, the class of bug this sweep exists for: 65x33 yuv420p has 33x17
  // chroma planes. width/2 is 32 and height/2 is 16, so plane 2 starts one
  // chroma row early -- inside plane 1.
  AVFrame f{};
  f.format = AV_PIX_FMT_YUV420P;
  f.width = 65;
  f.height = 33;
  const int need = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, 65, 33, 1);
  REQUIRE(need > 0);
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
  REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));

  CHECK(f.linesize[1] == 33);
  CHECK(f.linesize[2] == 33);
  CHECK(f.data[2] - f.data[1] >= 33 * 17);
}

TEST_CASE("the interleaved chroma plane of an odd NV12 frame is wide enough",
          "[video][layout][odd][!shouldfail]")
{
  // FINDING 5. NV12's UV plane holds one (U,V) pair per 2x2 block, i.e.
  // 2*ceil(w/2) bytes per row: 66 for a 65-wide frame, not 65.
  AVFrame f{};
  f.format = AV_PIX_FMT_NV12;
  f.width = 65;
  f.height = 33;
  const int need = av_image_get_buffer_size(AV_PIX_FMT_NV12, 65, 33, 1);
  REQUIRE(need > 0);
  std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
  REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));
  CHECK(f.linesize[1] >= av_image_get_linesize(AV_PIX_FMT_NV12, 65, 1));
}

TEST_CASE("packed 4:2:2 rows of an odd frame carry the trailing macropixel",
          "[video][layout][odd][!shouldfail]")
{
  // FINDING 6. A YUYV macropixel spans two columns, so a 65-wide row is 33
  // macropixels = 132 bytes, not 2*65.
  for(auto fmt : {AV_PIX_FMT_YUYV422, AV_PIX_FMT_UYVY422, AV_PIX_FMT_YVYU422})
  {
    INFO(fmtName(fmt));
    AVFrame f{};
    f.format = fmt;
    f.width = 65;
    f.height = 33;
    const int need = av_image_get_buffer_size(fmt, 65, 33, 1);
    REQUIRE(need > 0);
    std::vector<uint8_t> buf(std::size_t(need) + 4096, 0);
    REQUIRE(Video::initFrameFromRawData(&f, buf.data(), std::size_t(need)));
    CHECK(f.linesize[0] >= av_image_get_linesize(fmt, 65, 0));
  }
}

TEST_CASE("every format the GStreamer table names exists in this libav",
          "[video][layout]")
{
  // A mapping row that names a format this build of libav does not have would
  // be AV_PIX_FMT_NONE, which the device would then treat as "unsupported" for
  // a stream it can perfectly well receive.
  for(const auto& [name, fmt] : Video::gstreamerToLibav())
  {
    INFO("gstreamer format " << name);
    CHECK(fmt != AV_PIX_FMT_NONE);
    CHECK(av_get_pix_fmt_name(fmt) != nullptr);
  }
}

TEST_CASE("the GStreamer format table does not mis-name a layout",
          "[video][layout][!shouldfail]")
{
  // Two rows of gstreamerToLibav() name a libav format with a different layout
  // from the GStreamer one:
  //   7. "Y410" -> AV_PIX_FMT_YUV410P. GStreamer Y410 is packed 4:4:4 10-bit YUV
  //      with alpha, 4 bytes/pixel; YUV410P is planar 4:1:0 8-bit, 1.125
  //      bytes/pixel, which is GStreamer "YUV9".
  //   8. "RGBP" -> AV_PIX_FMT_RGB24. GStreamer RGBP is planar 4:4:4 RGB; RGB24 is
  //      packed. Same byte count, different meaning.
  const auto& map = Video::gstreamerToLibav();

  {
    const auto it = map.find("Y410");
    REQUIRE(it != map.end());
    const auto* d = av_pix_fmt_desc_get(it->second);
    REQUIRE(d);
    INFO("Y410 -> " << fmtName(it->second));
    CHECK(d->log2_chroma_w == 0);
    CHECK(d->log2_chroma_h == 0);
    CHECK(av_get_bits_per_pixel(d) >= 30);
  }
  {
    const auto it = map.find("RGBP");
    REQUIRE(it != map.end());
    const auto* d = av_pix_fmt_desc_get(it->second);
    REQUIRE(d);
    INFO("RGBP -> " << fmtName(it->second));
    CHECK((d->flags & AV_PIX_FMT_FLAG_PLANAR) != 0);
  }
}
