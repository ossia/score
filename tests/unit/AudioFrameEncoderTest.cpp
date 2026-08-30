// Gfx::AudioFrameEncoder and its ten concrete forms: the conversion that turns
// the audio engine's planar float buffers into the sample layout libav's
// encoder was opened with (Gfx/Libav/LibavOutputStream.hpp picks one per
// AVSampleFormat).
//
// Nothing here needs a codec, a muxer or a graphics device: an AVFrame whose
// buffers came from av_frame_get_buffer is the whole environment. What the
// suite pins is exactly what a wrong conversion sounds like:
//
//  - INTERLEAVE ORDER. The packed forms write frame-major, channel-minor
//    (out[i * channels + c]). Transposing that is silent on mono and swaps the
//    stereo image on everything else, which no "a file was produced" check
//    sees.
//  - PLANE ROUTING. The planar forms write channel i into data[i], and into
//    extended_data[i] past AV_NUM_DATA_POINTERS. A >8-channel frame is the only
//    thing that exercises the second branch, so one is built.
//  - THE SAMPLE VALUES THEMSELVES, as literals computed by hand from the
//    quantiser, not by re-running it. Truncation direction is part of the
//    contract: ossia::float_to_sample rounds toward zero, so +0.5 and -0.5 are
//    NOT symmetric (16383 vs -16384) and asserting a symmetric pair would be
//    asserting the wrong thing.
//  - THE LENGTH. AudioFrameEncoder::writable() is what keeps a tick that
//    carries more samples than the codec's frame_size from running off the end
//    of buffers that were sized once, at avcodec_open2 time. Every case fills
//    the frame with a sentinel first and checks the bytes past the write.
//
// Two full-scale cases are registered [!shouldfail]: they assert the correct
// value and the conversion does not produce it. See their comments.

#include <Media/Libav.hpp>

#include <catch2/catch_test_macros.hpp>

#if SCORE_HAS_LIBAV
#include <Gfx/Libav/AudioFrameEncoder.hpp>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

using namespace Gfx;

namespace
{
constexpr uint8_t kSentinel = 0xA5;

// An AVFrame with real, engine-allocated buffers, pre-poisoned so that any byte
// an encoder did not write is recognisable.
struct TestFrame
{
  AVFrame* f{};

  TestFrame(AVSampleFormat fmt, int channels, int nb_samples)
  {
    f = av_frame_alloc();
    REQUIRE(f != nullptr);
    f->format = fmt;
    f->nb_samples = nb_samples;
    f->sample_rate = 48000;
    av_channel_layout_default(&f->ch_layout, channels);
    REQUIRE(av_frame_get_buffer(f, 0) >= 0);

    const int planes = av_sample_fmt_is_planar(fmt) ? channels : 1;
    for(int i = 0; i < planes; i++)
    {
      auto* p = (i < AV_NUM_DATA_POINTERS) ? f->data[i] : f->extended_data[i];
      REQUIRE(p != nullptr);
      std::memset(p, kSentinel, f->linesize[0]);
    }
  }
  ~TestFrame() { av_frame_free(&f); }
  TestFrame(const TestFrame&) = delete;
  TestFrame& operator=(const TestFrame&) = delete;

  uint8_t* plane(int i) const
  {
    return (i < AV_NUM_DATA_POINTERS) ? f->data[i] : f->extended_data[i];
  }

  // True when every byte of plane `i` from `fromByte` to the end of the
  // allocation is still the sentinel.
  bool untouchedFrom(int i, int fromByte) const
  {
    const uint8_t* p = plane(i);
    for(int b = fromByte; b < f->linesize[0]; b++)
      if(p[b] != kSentinel)
        return false;
    return true;
  }
};

// Channel-major float input, the shape the audio engine hands over.
std::vector<ossia::float_vector> channels(std::vector<std::vector<float>> in)
{
  std::vector<ossia::float_vector> out;
  out.reserve(in.size());
  for(auto& ch : in)
  {
    ossia::float_vector v;
    v.resize(ch.size());
    for(std::size_t i = 0; i < ch.size(); i++)
      v[i] = ch[i];
    out.push_back(std::move(v));
  }
  return out;
}
}

TEST_CASE("AudioFrameEncoder::writable bounds the write", "[gfx][libav][audioenc]")
{
  // Vector shorter than the frame: the tick produced less than the codec's
  // frame_size, so only what exists may be written.
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 2, 1024};
    auto v = channels({std::vector<float>(300, 0.f), std::vector<float>(300, 0.f)});
    CHECK(AudioFrameEncoder::writable(*fr.f, v) == 300);
  }

  // Vector longer than the frame: the buffers were allocated for nb_samples and
  // that is the hard ceiling.
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 2, 64};
    auto v = channels({std::vector<float>(4096, 0.f), std::vector<float>(4096, 0.f)});
    CHECK(AudioFrameEncoder::writable(*fr.f, v) == 64);
  }

  // Equal.
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 2, 128};
    auto v = channels({std::vector<float>(128, 0.f), std::vector<float>(128, 0.f)});
    CHECK(AudioFrameEncoder::writable(*fr.f, v) == 128);
  }

  // A frame libav never gave buffers to. Returning nb_samples here would be a
  // write through frame.data[0] == nullptr.
  {
    AVFrame* raw = av_frame_alloc();
    raw->nb_samples = 0;
    auto v = channels({std::vector<float>(64, 0.f)});
    CHECK(AudioFrameEncoder::writable(*raw, v) == 0);
    raw->nb_samples = -1;
    CHECK(AudioFrameEncoder::writable(*raw, v) == 0);
    av_frame_free(&raw);
  }
}

TEST_CASE("Interleaved encoders write frame-major, channel-minor", "[gfx][libav][audioenc]")
{
  // Three channels whose values are trivially distinguishable, so a transposed
  // write cannot look right by coincidence.
  auto v = channels(
      {{0.f, 0.25f, 0.5f}, {-0.25f, -0.25f, -0.25f}, {-0.5f, -0.5f, -0.5f}});

  SECTION("S16 interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 3, 3};
    S16IAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const int16_t*>(fr.f->data[0]);
    // 0.f -> -0.5 -> 0 ; 0.25 -> 8191.375 -> 8191 ; 0.5 -> 16383.25 -> 16383
    // -0.25 -> -8192.375 -> -8192 ; -0.5 -> -16384.25 -> -16384
    CHECK(o[0] == 0);     CHECK(o[1] == -8192); CHECK(o[2] == -16384);
    CHECK(o[3] == 8191);  CHECK(o[4] == -8192); CHECK(o[5] == -16384);
    CHECK(o[6] == 16383); CHECK(o[7] == -8192); CHECK(o[8] == -16384);
    CHECK(fr.untouchedFrom(0, 9 * 2));
  }

  SECTION("S32 interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_S32, 3, 3};
    S32IAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const int32_t*>(fr.f->data[0]);
    // Same mapping as S16 above, one bit depth up: x * (max + 0.5) - 0.5, so
    // the positive levels land one below the symmetric scale and the negative
    // ones are unchanged.
    CHECK(o[0] == 0);          CHECK(o[1] == -536870912); CHECK(o[2] == -1073741824);
    CHECK(o[3] == 536870911);  CHECK(o[4] == -536870912); CHECK(o[5] == -1073741824);
    CHECK(o[6] == 1073741823); CHECK(o[7] == -536870912); CHECK(o[8] == -1073741824);
    CHECK(fr.untouchedFrom(0, 9 * 4));
  }

  SECTION("S24 interleaved (int32 carrier)")
  {
    // No AVSampleFormat is 24-bit: libav's pcm_s24le takes S32 samples. The
    // frame is allocated as S32 for that reason; what differs is the scale the
    // encoder applies, 2^23 instead of 2^31.
    TestFrame fr{AV_SAMPLE_FMT_S32, 3, 3};
    S24IAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const int32_t*>(fr.f->data[0]);
    CHECK(o[0] == 0);        CHECK(o[1] == -2097152); CHECK(o[2] == -4194304);
    CHECK(o[3] == 2097151);  CHECK(o[4] == -2097152); CHECK(o[5] == -4194304);
    CHECK(o[6] == 4194303);  CHECK(o[7] == -2097152); CHECK(o[8] == -4194304);
    CHECK(fr.untouchedFrom(0, 9 * 4));
  }

  SECTION("Float interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_FLT, 3, 3};
    FltIAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const float*>(fr.f->data[0]);
    CHECK(o[0] == 0.f);    CHECK(o[1] == -0.25f); CHECK(o[2] == -0.5f);
    CHECK(o[3] == 0.25f);  CHECK(o[4] == -0.25f); CHECK(o[5] == -0.5f);
    CHECK(o[6] == 0.5f);   CHECK(o[7] == -0.25f); CHECK(o[8] == -0.5f);
    CHECK(fr.untouchedFrom(0, 9 * 4));
  }

  SECTION("Double interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_DBL, 3, 3};
    DblIAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const double*>(fr.f->data[0]);
    CHECK(o[0] == 0.);    CHECK(o[1] == -0.25); CHECK(o[2] == -0.5);
    CHECK(o[3] == 0.25);  CHECK(o[4] == -0.25); CHECK(o[5] == -0.5);
    CHECK(o[6] == 0.5);   CHECK(o[7] == -0.25); CHECK(o[8] == -0.5);
    CHECK(fr.untouchedFrom(0, 9 * 8));
  }
}

TEST_CASE("Planar encoders route channel i to plane i", "[gfx][libav][audioenc]")
{
  auto v = channels(
      {{0.f, 0.25f, 0.5f}, {-0.25f, -0.25f, -0.25f}, {-0.5f, -0.5f, -0.5f}});

  SECTION("S16 planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_S16P, 3, 3};
    S16PAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* c0 = reinterpret_cast<const int16_t*>(fr.plane(0));
    const auto* c1 = reinterpret_cast<const int16_t*>(fr.plane(1));
    const auto* c2 = reinterpret_cast<const int16_t*>(fr.plane(2));
    CHECK(c0[0] == 0); CHECK(c0[1] == 8191); CHECK(c0[2] == 16383);
    CHECK(c1[0] == -8192); CHECK(c1[1] == -8192); CHECK(c1[2] == -8192);
    CHECK(c2[0] == -16384); CHECK(c2[1] == -16384); CHECK(c2[2] == -16384);
    for(int i = 0; i < 3; i++)
      CHECK(fr.untouchedFrom(i, 3 * 2));
  }

  SECTION("S32 planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_S32P, 3, 3};
    S32PAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* c0 = reinterpret_cast<const int32_t*>(fr.plane(0));
    const auto* c1 = reinterpret_cast<const int32_t*>(fr.plane(1));
    const auto* c2 = reinterpret_cast<const int32_t*>(fr.plane(2));
    CHECK(c0[0] == 0); CHECK(c0[1] == 536870911); CHECK(c0[2] == 1073741823);
    CHECK(c1[0] == -536870912); CHECK(c1[2] == -536870912);
    CHECK(c2[0] == -1073741824); CHECK(c2[2] == -1073741824);
    for(int i = 0; i < 3; i++)
      CHECK(fr.untouchedFrom(i, 3 * 4));
  }

  SECTION("Float planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_FLTP, 3, 3};
    FltPAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* c0 = reinterpret_cast<const float*>(fr.plane(0));
    const auto* c1 = reinterpret_cast<const float*>(fr.plane(1));
    const auto* c2 = reinterpret_cast<const float*>(fr.plane(2));
    CHECK(c0[0] == 0.f); CHECK(c0[1] == 0.25f); CHECK(c0[2] == 0.5f);
    CHECK(c1[0] == -0.25f); CHECK(c1[2] == -0.25f);
    CHECK(c2[0] == -0.5f); CHECK(c2[2] == -0.5f);
    for(int i = 0; i < 3; i++)
      CHECK(fr.untouchedFrom(i, 3 * 4));
  }

  SECTION("Double planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_DBLP, 3, 3};
    DblPAudioFrameEncoder{3}.add_frame(*fr.f, v);
    const auto* c0 = reinterpret_cast<const double*>(fr.plane(0));
    const auto* c1 = reinterpret_cast<const double*>(fr.plane(1));
    const auto* c2 = reinterpret_cast<const double*>(fr.plane(2));
    CHECK(c0[0] == 0.); CHECK(c0[1] == 0.25); CHECK(c0[2] == 0.5);
    CHECK(c1[0] == -0.25); CHECK(c1[2] == -0.25);
    CHECK(c2[0] == -0.5); CHECK(c2[2] == -0.5);
    for(int i = 0; i < 3; i++)
      CHECK(fr.untouchedFrom(i, 3 * 8));
  }
}

TEST_CASE(
    "Planar encoders reach channels past AV_NUM_DATA_POINTERS",
    "[gfx][libav][audioenc]")
{
  // frame.data[] holds 8 pointers; a 10-channel planar frame keeps channels 8
  // and 9 only in extended_data. An encoder that indexed data[] unconditionally
  // would write through a null pointer, or silently drop the last channels.
  constexpr int N = 10;
  REQUIRE(N > AV_NUM_DATA_POINTERS);

  std::vector<std::vector<float>> raw;
  for(int c = 0; c < N; c++)
    raw.push_back({float(c) / 32.f, float(c) / 32.f});
  auto v = channels(raw);

  SECTION("Float planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_FLTP, N, 2};
    REQUIRE(fr.f->extended_data != nullptr);
    FltPAudioFrameEncoder{2}.add_frame(*fr.f, v);
    for(int c = 0; c < N; c++)
    {
      const auto* p = reinterpret_cast<const float*>(fr.plane(c));
      INFO("channel " << c);
      CHECK(p[0] == float(c) / 32.f);
      CHECK(p[1] == float(c) / 32.f);
    }
  }

  SECTION("S16 planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_S16P, N, 2};
    S16PAudioFrameEncoder{2}.add_frame(*fr.f, v);
    // c/32 * 32767.5 - 0.5, truncated toward zero.
    const int16_t expected[N]
        = {0, 1023, 2047, 3071, 4095, 5119, 6143, 7167, 8191, 9215};
    for(int c = 0; c < N; c++)
    {
      const auto* p = reinterpret_cast<const int16_t*>(fr.plane(c));
      INFO("channel " << c);
      CHECK(p[0] == expected[c]);
    }
  }

  SECTION("S32 planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_S32P, N, 2};
    S32PAudioFrameEncoder{2}.add_frame(*fr.f, v);
    // c/32 * 2147483647.5 - 0.5, truncated toward zero.
    const int32_t expected[N]
        = {0,         67108863,  134217727, 201326591, 268435455,
           335544319, 402653183, 469762047, 536870911, 603979775};
    for(int c = 0; c < N; c++)
    {
      const auto* p = reinterpret_cast<const int32_t*>(fr.plane(c));
      INFO("channel " << c);
      CHECK(p[0] == expected[c]);
    }
  }

  SECTION("Double planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_DBLP, N, 2};
    DblPAudioFrameEncoder{2}.add_frame(*fr.f, v);
    for(int c = 0; c < N; c++)
    {
      const auto* p = reinterpret_cast<const double*>(fr.plane(c));
      INFO("channel " << c);
      CHECK(p[0] == double(float(c) / 32.f));
    }
  }
}

TEST_CASE(
    "A tick shorter than the codec frame leaves the tail alone",
    "[gfx][libav][audioenc]")
{
  // The engine's buffer size and the codec's frame_size disagree constantly.
  // Under-run must stop at what the tick produced; the rest of the frame keeps
  // whatever was there (libav is told how many samples are valid separately).
  auto v = channels({{0.5f, 0.5f}, {-0.5f, -0.5f}});

  SECTION("interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 2, 64};
    S16IAudioFrameEncoder{64}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const int16_t*>(fr.f->data[0]);
    CHECK(o[0] == 16383); CHECK(o[1] == -16384);
    CHECK(o[2] == 16383); CHECK(o[3] == -16384);
    CHECK(fr.untouchedFrom(0, 4 * 2));
  }

  SECTION("planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_FLTP, 2, 64};
    FltPAudioFrameEncoder{64}.add_frame(*fr.f, v);
    const auto* c0 = reinterpret_cast<const float*>(fr.plane(0));
    CHECK(c0[0] == 0.5f); CHECK(c0[1] == 0.5f);
    CHECK(fr.untouchedFrom(0, 2 * 4));
    CHECK(fr.untouchedFrom(1, 2 * 4));
  }
}

TEST_CASE(
    "A tick longer than the codec frame does not overrun", "[gfx][libav][audioenc]")
{
  // The other direction, and the one that corrupts the heap rather than the
  // sound: 4096 samples arriving for a frame allocated with nb_samples = 8.
  auto v = channels({std::vector<float>(4096, 0.5f), std::vector<float>(4096, -0.5f)});

  SECTION("interleaved")
  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 2, 8};
    S16IAudioFrameEncoder{8}.add_frame(*fr.f, v);
    const auto* o = reinterpret_cast<const int16_t*>(fr.f->data[0]);
    for(int i = 0; i < 16; i++)
      CHECK(o[i] == (i % 2 == 0 ? 16383 : -16384));
    CHECK(fr.untouchedFrom(0, 16 * 2));
  }

  SECTION("planar")
  {
    TestFrame fr{AV_SAMPLE_FMT_S32P, 2, 8};
    S32PAudioFrameEncoder{8}.add_frame(*fr.f, v);
    for(int p = 0; p < 2; p++)
    {
      const auto* o = reinterpret_cast<const int32_t*>(fr.plane(p));
      for(int i = 0; i < 8; i++)
        CHECK(o[i] == (p == 0 ? 1073741823 : -1073741824));
      CHECK(fr.untouchedFrom(p, 8 * 4));
    }
  }
}

TEST_CASE("Mono is not special-cased away", "[gfx][libav][audioenc]")
{
  auto v = channels({{0.5f, -0.5f, 0.25f}});
  TestFrame fr{AV_SAMPLE_FMT_S16, 1, 3};
  S16IAudioFrameEncoder{3}.add_frame(*fr.f, v);
  const auto* o = reinterpret_cast<const int16_t*>(fr.f->data[0]);
  CHECK(o[0] == 16383);
  CHECK(o[1] == -16384);
  CHECK(o[2] == 8191);
  CHECK(fr.untouchedFrom(0, 3 * 2));
}

TEST_CASE("Negative full scale converts exactly", "[gfx][libav][audioenc]")
{
  // -1.0 is the one endpoint every integer form gets right, and it is worth
  // pinning because it is what makes the positive-endpoint failures below
  // asymmetric rather than a uniform off-by-one.
  auto v = channels({{-1.f}});

  {
    TestFrame fr{AV_SAMPLE_FMT_S16, 1, 1};
    S16IAudioFrameEncoder{1}.add_frame(*fr.f, v);
    CHECK(reinterpret_cast<const int16_t*>(fr.f->data[0])[0] == -32768);
  }
  {
    TestFrame fr{AV_SAMPLE_FMT_S32, 1, 1};
    S32IAudioFrameEncoder{1}.add_frame(*fr.f, v);
    CHECK(reinterpret_cast<const int32_t*>(fr.f->data[0])[0] == -2147483647 - 1);
  }
  {
    TestFrame fr{AV_SAMPLE_FMT_S32, 1, 1};
    S24IAudioFrameEncoder{1}.add_frame(*fr.f, v);
    CHECK(reinterpret_cast<const int32_t*>(fr.f->data[0])[0] == -8388608);
  }
}

TEST_CASE("Positive full scale converts exactly", "[gfx][libav][audioenc]")
{
  // The one endpoint that is right for S16 too: 1.0 * 32767.5 - 0.5 == 32767.
  auto v = channels({{1.f}});
  TestFrame fr{AV_SAMPLE_FMT_S16, 1, 1};
  S16IAudioFrameEncoder{1}.add_frame(*fr.f, v);
  CHECK(reinterpret_cast<const int16_t*>(fr.f->data[0])[0] == 32767);
}

TEST_CASE(
    "S32 encoders survive a full-scale positive sample",
    "[gfx][libav][audioenc]")
{
  // This used to multiply by (float)INT32_MAX, which rounds UP to 2147483648.0f
  // -- one past the largest representable int32 -- so converting 1.0f was out of
  // range and cvttss2si returned INT32_MIN. A full-scale positive peak came out
  // as a full-scale NEGATIVE one in every AV_SAMPLE_FMT_S32 / S32P stream, which
  // is what libav's 24- and 32-bit PCM encoders take.
  auto v = channels({{1.f}});

  TestFrame interleaved{AV_SAMPLE_FMT_S32, 1, 1};
  S32IAudioFrameEncoder{1}.add_frame(*interleaved.f, v);
  CHECK(reinterpret_cast<const int32_t*>(interleaved.f->data[0])[0] == 2147483647);

  TestFrame planar{AV_SAMPLE_FMT_S32P, 1, 1};
  S32PAudioFrameEncoder{1}.add_frame(*planar.f, v);
  CHECK(reinterpret_cast<const int32_t*>(planar.plane(0))[0] == 2147483647);
}

TEST_CASE(
    "S24 encoder keeps a full-scale sample inside 24 bits",
    "[gfx][libav][audioenc]")
{
  // int24_max was INT32_MAX / 256.0 == 8388607.99609375, which as a float rounds
  // UP to 2^23, so 1.0 quantised to 8388608 -- one past the largest signed
  // 24-bit value, reading back as -8388608. The same polarity flip as the S32
  // case above, one bit depth down.
  auto v = channels({{1.f}});
  TestFrame fr{AV_SAMPLE_FMT_S32, 1, 1};
  S24IAudioFrameEncoder{1}.add_frame(*fr.f, v);
  CHECK(reinterpret_cast<const int32_t*>(fr.f->data[0])[0] == 8388607);
}

#else
TEST_CASE("AudioFrameEncoder needs libav", "[gfx][libav][audioenc][!mayfail]")
{
  SKIP("built without libav");
}
#endif
