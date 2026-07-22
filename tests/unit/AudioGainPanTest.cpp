#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

namespace
{
void fill(ossia::audio_channel& chan, std::initializer_list<double> vals)
{
  chan.assign(vals.begin(), vals.end());
}
}

TEST_CASE("audio_outlet::post_process applies the outlet gain to a mono channel", "[audio][gain]")
{
  ossia::audio_outlet out;
  out->set_channels(1);
  fill(out->channel(0), {1., -2., 0.5, 0., 1e-30});

  SECTION("gain 0.5 halves every sample, in place")
  {
    out.gain = 0.5;
    out.post_process();
    CHECK(out->channel(0)[0] == 0.5);
    CHECK(out->channel(0)[1] == -1.0);
    CHECK(out->channel(0)[2] == 0.25);
    CHECK(out->channel(0)[3] == 0.0);
    CHECK(out->channel(0)[4] == 0.5e-30);
  }

  SECTION("unity gain leaves samples bit-identical (fast path)")
  {
    out.gain = 1.;
    out.post_process();
    CHECK(out->channel(0)[0] == 1.);
    CHECK(out->channel(0)[1] == -2.);
    CHECK(out->channel(0)[2] == 0.5);
  }

  SECTION("zero gain silences everything")
  {
    out.gain = 0.;
    out.post_process();
    for(auto s : out->channel(0))
      CHECK(s == 0.0);
  }

  SECTION("NaN samples do not crash the gain stage")
  {
    out->channel(0)[1] = std::numeric_limits<double>::quiet_NaN();
    out.gain = 2.;
    out.post_process();
    CHECK(out->channel(0)[0] == 2.);
    CHECK(std::isnan(out->channel(0)[1]));
  }
}

TEST_CASE("audio_outlet::post_process applies per-channel pan * gain on stereo", "[audio][pan]")
{
  ossia::audio_outlet out;
  out->set_channels(2);
  fill(out->channel(0), {1., 2., 3., 4.});
  fill(out->channel(1), {4., 3., 2., 1.});

  SECTION("default pan {1,1} + unity gain: passthrough")
  {
    out.post_process();
    CHECK(out->channel(0)[0] == 1.);
    CHECK(out->channel(1)[0] == 4.);
  }

  SECTION("pan weights scale each channel independently")
  {
    out.pan = {0.25, 0.75};
    out.gain = 1.;
    out.post_process();
    for(int i = 0; i < 4; i++)
    {
      CHECK(out->channel(0)[i] == (i + 1.) * 0.25);
      CHECK(out->channel(1)[i] == (4. - i) * 0.75);
    }
  }

  SECTION("gain multiplies on top of the pan weights: vol = pan[c] * gain")
  {
    out.pan = {0.25, 0.75};
    out.gain = 2.;
    out.post_process();
    for(int i = 0; i < 4; i++)
    {
      CHECK(out->channel(0)[i] == (i + 1.) * 0.5);
      CHECK(out->channel(1)[i] == (4. - i) * 1.5);
    }
  }

  SECTION("hard-left pan {1,0} silences the right channel exactly")
  {
    out.pan = {1., 0.};
    out.post_process();
    for(int i = 0; i < 4; i++)
    {
      CHECK(out->channel(0)[i] == i + 1.);
      CHECK(out->channel(1)[i] == 0.0);
    }
  }
}

TEST_CASE("audio_outlet::post_process extends the pan vector for >2 channels", "[audio][pan]")
{
  ossia::audio_outlet out;
  out->set_channels(4);
  for(std::size_t c = 0; c < 4; c++)
    fill(out->channel(c), {1., 1.});

  out.pan = {0.5, 0.25}; // channels 2,3 have no explicit pan weight
  out.gain = 2.;
  out.post_process();

  // pan is auto-extended with 1. for the extra channels
  REQUIRE(out.pan.size() == 4);
  CHECK(out.pan[2] == 1.);
  CHECK(out.pan[3] == 1.);
  CHECK(out->channel(0)[0] == 1.0); // 1 * 0.5 * 2
  CHECK(out->channel(1)[0] == 0.5); // 1 * 0.25 * 2
  CHECK(out->channel(2)[0] == 2.0); // 1 * 1 * 2
  CHECK(out->channel(3)[0] == 2.0);
}

TEST_CASE("audio_outlet::post_process reads the gain from the gain inlet messages", "[audio][gain]")
{
  ossia::audio_outlet out;
  out->set_channels(1);
  fill(out->channel(0), {1., 2.});

  out.gain = 1.;
  (*out.gain_inlet).write_value(0.25f, 0);
  out.post_process();

  // The last gain message overrides the member before processing…
  CHECK(out.gain == 0.25);
  CHECK(out->channel(0)[0] == 0.25);
  CHECK(out->channel(0)[1] == 0.5);
}

TEST_CASE("audio_outlet::post_process edge cases", "[audio][gain][fuzz]")
{
  SECTION("no channels at all: early return, no crash")
  {
    ossia::audio_outlet out;
    out->set_channels(0);
    out.gain = 0.5;
    out.post_process();
    CHECK(out->channels() == 0);
  }

  SECTION("zero-length channels: no crash")
  {
    ossia::audio_outlet out;
    out->set_channels(2);
    out.gain = 0.5;
    out.pan = {0.5, 0.5};
    out.post_process();
    CHECK(out->channel(0).empty());
  }

  SECTION("single-sample channel")
  {
    ossia::audio_outlet out;
    out->set_channels(1);
    fill(out->channel(0), {2.});
    out.gain = 0.25;
    out.post_process();
    CHECK(out->channel(0)[0] == 0.5);
  }
}

TEST_CASE("ossia::mix is a sample-exact summing bus", "[audio][mix]")
{
  ossia::audio_port src, sink;

  SECTION("mix into an empty sink copies the source")
  {
    src.set_channels(2);
    fill(src.channel(0), {1., 2.});
    fill(src.channel(1), {3., 4.});
    sink.set_channels(0);

    ossia::mix(src.get(), sink.get());
    REQUIRE(sink.channels() == 2);
    CHECK(sink.channel(0)[0] == 1.);
    CHECK(sink.channel(0)[1] == 2.);
    CHECK(sink.channel(1)[0] == 3.);
    CHECK(sink.channel(1)[1] == 4.);
  }

  SECTION("mix into a matching sink adds sample-wise")
  {
    src.set_channels(2);
    fill(src.channel(0), {1., 2.});
    fill(src.channel(1), {3., 4.});
    sink.set_channels(2);
    fill(sink.channel(0), {10., 20.});
    fill(sink.channel(1), {30., 40.});

    ossia::mix(src.get(), sink.get());
    CHECK(sink.channel(0)[0] == 11.);
    CHECK(sink.channel(0)[1] == 22.);
    CHECK(sink.channel(1)[0] == 33.);
    CHECK(sink.channel(1)[1] == 44.);
  }

  SECTION("two sources summed = the sum of both (mixing is associative)")
  {
    src.set_channels(1);
    fill(src.channel(0), {1., -1.});
    ossia::audio_port src2;
    src2.set_channels(1);
    fill(src2.channel(0), {0.5, 0.5});
    sink.set_channels(1);
    fill(sink.channel(0), {0., 0.});

    ossia::mix(src.get(), sink.get());
    ossia::mix(src2.get(), sink.get());
    CHECK(sink.channel(0)[0] == 1.5);
    CHECK(sink.channel(0)[1] == -0.5);
  }

  SECTION("sink shorter than source is grown, samples preserved")
  {
    src.set_channels(1);
    fill(src.channel(0), {1., 2., 3.});
    sink.set_channels(1);
    fill(sink.channel(0), {10.});

    ossia::mix(src.get(), sink.get());
    REQUIRE(sink.channel(0).size() == 3);
    CHECK(sink.channel(0)[0] == 11.);
    CHECK(sink.channel(0)[1] == 2.);
    CHECK(sink.channel(0)[2] == 3.);
  }

  SECTION("empty source into empty sink: no crash")
  {
    ossia::mix(src.get(), sink.get());
    SUCCEED("no crash");
  }
}
