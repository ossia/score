// Video::gstreamerToLibav(): the GStreamer format-name -> AVPixelFormat table
// behind the GStreamer, Sh4lt and Shmdata inputs.
//
// Guards 649d291dad (A444_10LE was mapped to YUVA444P10BE) with the rule that
// would have caught it, swept over the whole table rather than pinned on the
// one row that was wrong.

#include <Video/GStreamerCompatibility.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <string_view>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace
{
bool endsWith(const std::string& s, std::string_view suffix)
{
  return s.size() >= suffix.size()
         && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

const AVPixFmtDescriptor* described(AVPixelFormat f)
{
  return av_pix_fmt_desc_get(f);
}

bool bigEndian(AVPixelFormat f)
{
  const auto* d = described(f);
  return d && (d->flags & AV_PIX_FMT_FLAG_BE);
}

std::string_view name(AVPixelFormat f)
{
  const char* n = av_get_pix_fmt_name(f);
  return n ? std::string_view{n} : std::string_view{};
}
}

TEST_CASE("A444_10LE maps to the little-endian FFmpeg format", "[unit][gstreamer][video]")
{
  const auto& map = Video::gstreamerToLibav();

  // Resolved through FFmpeg's own naming rather than compared to a retyped
  // AV_PIX_FMT_* enumerator, so the expectation does not come from the same
  // place as the value under test.
  REQUIRE(map.count("A444_10LE") == 1);
  REQUIRE(map.count("A444_10BE") == 1);
  CHECK(name(map.at("A444_10LE")) == std::string_view{"yuva444p10le"});
  CHECK(name(map.at("A444_10BE")) == std::string_view{"yuva444p10be"});
}

TEST_CASE("every endianness-suffixed name maps to that endianness", "[unit][gstreamer][video]")
{
  for(const auto& [key, fmt] : Video::gstreamerToLibav())
  {
    INFO("gstreamer format " << key << " -> " << name(fmt));
    if(endsWith(key, "LE"))
      CHECK_FALSE(bigEndian(fmt));
    else if(endsWith(key, "BE"))
      CHECK(bigEndian(fmt));
  }
}

TEST_CASE("an LE/BE pair never resolves to the same format", "[unit][gstreamer][video]")
{
  const auto& map = Video::gstreamerToLibav();
  int pairs = 0;
  for(const auto& [key, fmt] : map)
  {
    if(!endsWith(key, "LE"))
      continue;
    std::string be = key.substr(0, key.size() - 2) + "BE";
    auto it = map.find(be);
    if(it == map.end())
      continue;
    ++pairs;
    INFO(key << " / " << be << " both -> " << name(fmt));
    CHECK(fmt != it->second);
  }
  // The bug was one half of such a pair collapsing onto the other; if the
  // sweep finds no pairs it is asserting nothing.
  CHECK(pairs >= 10);
}

TEST_CASE("the unsuffixed multi-byte names are native-endian", "[unit][gstreamer][video]")
{
  const auto& map = Video::gstreamerToLibav();
  // AV_PIX_FMT_AYUV64 and AV_PIX_FMT_Y210 are AV_PIX_FMT_NE aliases: the only
  // two rows whose endianness follows the host rather than the name.
  CHECK(map.at("AYUV64") == AV_PIX_FMT_AYUV64);
  CHECK(map.at("Y210") == AV_PIX_FMT_Y210);
}

TEST_CASE("every mapped format is one the linked libav has", "[unit][gstreamer][video]")
{
  const auto& map = Video::gstreamerToLibav();
  for(const auto& [key, fmt] : map)
  {
    INFO("gstreamer format " << key);
    CHECK(fmt != AV_PIX_FMT_NONE);
    CHECK(described(fmt) != nullptr);
    CHECK_FALSE(name(fmt).empty());
  }
}

TEST_CASE("no two names claim the same layout by accident", "[unit][gstreamer][video]")
{
  // RGB and RGBP both answer rgb24 on purpose (GStreamer's planar RGB has no
  // libav twin here); every other collision would be a copy-paste.
  std::map<AVPixelFormat, std::vector<std::string>> byFormat;
  for(const auto& [key, fmt] : Video::gstreamerToLibav())
    byFormat[fmt].push_back(key);

  for(auto& [fmt, keys] : byFormat)
  {
    std::sort(keys.begin(), keys.end());
    if(keys.size() == 1)
      continue;
    INFO("format " << name(fmt) << " claimed by " << keys.size() << " names");
    CHECK(keys == std::vector<std::string>{"RGB", "RGBP"});
  }
}

TEST_CASE("the table still has every row it was written with", "[unit][gstreamer][video]")
{
  CHECK(Video::gstreamerToLibav().size() == 54);
}
