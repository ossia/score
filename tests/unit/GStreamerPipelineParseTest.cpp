// Appsink discovery and media-type classification from a GStreamer pipeline
// string (680dccbd29, part b).
//
// A live source has no negotiated caps when the pipeline is built; before this
// the device defaulted every unnegotiated appsink to 640x480 RGBA video, so an
// audio appsink came out as a video one.

#include <Gfx/GStreamer/GStreamerPipelineParse.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace Gfx::GStreamer;

namespace
{
AppsinkInfo classify(const std::string& pipeline, const std::string& sink)
{
  AppsinkInfo info;
  classify_from_pipeline_string(pipeline, sink, info);
  return info;
}
}

TEST_CASE("named appsinks are found, other named elements are not", "[unit][gstreamer]")
{
  const std::string p = "videotestsrc ! videoconvert ! appsink name=vsink ! "
                        "audiotestsrc ! appsink name=asink";
  CHECK(find_appsink_names(p) == std::vector<std::string>{"vsink", "asink"});

  CHECK(find_appsink_names("videotestsrc ! appsink").empty());
  CHECK(find_appsink_names("videotestsrc ! videoconvert name=cv ! appsink").empty());

  CHECK(
      find_all_named_elements("videotestsrc ! videoconvert name=cv ! appsink name=s")
      == std::vector<std::string>{"cv", "s"});
}

TEST_CASE("an appsink's media type follows the nearest preceding token", "[unit][gstreamer]")
{
  const std::string vfirst
      = "videotestsrc ! video/x-raw,width=1280,height=720 ! appsink name=v ! "
        "audiotestsrc ! audio/x-raw,channels=4,rate=44100 ! appsink name=a";

  {
    auto v = classify(vfirst, "v");
    CHECK(v.is_video);
    CHECK(v.width == 1280);
    CHECK(v.height == 720);
  }
  {
    auto a = classify(vfirst, "a");
    CHECK_FALSE(a.is_video);
    CHECK(a.channels == 4);
    CHECK(a.rate == 44100);
  }

  // Same two branches, order swapped: the answer must follow the position of
  // the sink in the string, not the order the tokens are declared in.
  const std::string afirst
      = "audiotestsrc ! audio/x-raw,channels=4,rate=44100 ! appsink name=a ! "
        "videotestsrc ! video/x-raw,width=1280,height=720 ! appsink name=v";
  {
    auto a = classify(afirst, "a");
    CHECK_FALSE(a.is_video);
    CHECK(a.channels == 4);
  }
  {
    auto v = classify(afirst, "v");
    CHECK(v.is_video);
    CHECK(v.width == 1280);
    CHECK(v.height == 720);
  }
}

TEST_CASE("an element token classifies as well as a caps token", "[unit][gstreamer]")
{
  CHECK(classify("audiotestsrc ! appsink name=s", "s").is_video == false);
  CHECK(classify("videotestsrc ! appsink name=s", "s").is_video == true);
  CHECK(classify("something ! audioconvert ! appsink name=s", "s").is_video == false);
  CHECK(classify("something ! audioresample ! appsink name=s", "s").is_video == false);
  CHECK(classify("something ! videoconvert ! appsink name=s", "s").is_video == true);
  CHECK(classify("something ! videoscale ! appsink name=s", "s").is_video == true);
  CHECK(classify("something ! videorate ! appsink name=s", "s").is_video == true);
}

TEST_CASE("an unclassifiable appsink falls back to 640x480 RGBA video", "[unit][gstreamer]")
{
  auto info = classify("filesrc location=x ! decodebin ! appsink name=s", "s");
  CHECK(info.is_video);
  CHECK(info.width == 640);
  CHECK(info.height == 480);
  CHECK(info.pixfmt == AV_PIX_FMT_RGBA);

  auto audio = classify("audiotestsrc ! appsink name=s", "s");
  CHECK_FALSE(audio.is_video);
  CHECK(audio.channels == 2);
  CHECK(audio.rate == 48000);
}

TEST_CASE("caps type annotations parse like the bare spellings", "[unit][gstreamer]")
{
  auto a = classify(
      "audiotestsrc ! audio/x-raw,channels=(int)6,rate=(int)96000 ! appsink name=s", "s");
  CHECK_FALSE(a.is_video);
  CHECK(a.channels == 6);
  CHECK(a.rate == 96000);

  auto v = classify(
      "videotestsrc ! video/x-raw,width=(int)3840,height=(int)2160,format=(string)NV12 "
      "! appsink name=s",
      "s");
  CHECK(v.is_video);
  CHECK(v.width == 3840);
  CHECK(v.height == 2160);
  CHECK(v.pixfmt == AV_PIX_FMT_NV12);
}

TEST_CASE("format= resolves through the gstreamer/libav table", "[unit][gstreamer]")
{
  CHECK(
      classify("videotestsrc ! video/x-raw,format=I420 ! appsink name=s", "s").pixfmt
      == AV_PIX_FMT_YUV420P);
  CHECK(
      classify("videotestsrc ! video/x-raw,format=BGRA ! appsink name=s", "s").pixfmt
      == AV_PIX_FMT_BGRA);
  // An unknown name leaves the fallback rather than yielding AV_PIX_FMT_NONE.
  CHECK(
      classify("videotestsrc ! video/x-raw,format=NOSUCH ! appsink name=s", "s").pixfmt
      == AV_PIX_FMT_RGBA);
  // Case-sensitive: the table is spelled the way GStreamer spells it.
  CHECK(
      classify("videotestsrc ! video/x-raw,format=nv12 ! appsink name=s", "s").pixfmt
      == AV_PIX_FMT_RGBA);
}

TEST_CASE("the last value before the sink wins", "[unit][gstreamer]")
{
  auto v = classify(
      "videotestsrc ! video/x-raw,width=640,height=480 ! videoscale ! "
      "video/x-raw,width=1920,height=1080,format=I420 ! appsink name=s ! "
      "video/x-raw,width=320 ! appsink name=later",
      "s");
  CHECK(v.width == 1920);
  CHECK(v.height == 1080);
  CHECK(v.pixfmt == AV_PIX_FMT_YUV420P);
}

TEST_CASE("a value that does not fit an int leaves the previous one", "[unit][gstreamer]")
{
  // ossia::parse_strict rejects the overflow, and last_int_of keeps what it had
  // rather than storing a truncated or garbage width.
  auto v = classify(
      "videotestsrc ! video/x-raw,width=1920 ! video/x-raw,width=99999999999 ! "
      "appsink name=s",
      "s");
  CHECK(v.width == 1920);

  // A non-numeric value never matches the pattern at all.
  auto w = classify(
      "videotestsrc ! video/x-raw,width=1920 ! video/x-raw,width=wide ! appsink name=s",
      "s");
  CHECK(w.width == 1920);
}

TEST_CASE("a sink name that is not in the pipeline scans the whole string", "[unit][gstreamer]")
{
  // sink_pos stays npos, so substr(0, npos) is the entire pipeline: an unknown
  // name is classified against everything rather than against nothing.
  auto info = classify("audiotestsrc ! audio/x-raw,channels=8 ! appsink name=s", "absent");
  CHECK_FALSE(info.is_video);
  CHECK(info.channels == 8);
}

TEST_CASE("framerate= never contributes to an audio sink's rate", "[unit][gstreamer]")
{
  // The rate= pattern requires a property boundary, so "framerate=30/1" in a
  // preceding video branch does not open the audio sink at 30 Hz.
  CHECK(
      classify("audiotestsrc ! audio/x-raw,framerate=30/1 ! appsink name=s", "s").rate
      == 48000);
  CHECK(
      classify(
          "audiotestsrc ! audio/x-raw,framerate=(fraction)30/1 ! appsink name=s", "s")
          .rate
      == 48000);
  // A real rate still parses, at the string start, after a comma and in
  // GStreamer's (int) spelling.
  CHECK(
      classify("audiotestsrc ! audio/x-raw,rate=44100 ! appsink name=s", "s").rate
      == 44100);
  CHECK(
      classify("audiotestsrc ! audio/x-raw,rate=(int)22050 ! appsink name=s", "s").rate
      == 22050);
  CHECK(
      classify(
          "videotestsrc ! video/x-raw,framerate=30/1 ! appsink name=v audiotestsrc ! "
          "audio/x-raw,rate=96000 ! appsink name=s",
          "s")
          .rate
      == 96000);
}
