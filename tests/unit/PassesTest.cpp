// A take recorded over passes of the same length, cut into its passes
// (Recording::passesOf and the functions that cut a take):
//  - passes are counted from the start of the take; an incomplete last one is
//    left out, unless there is no complete one;
//  - the content of a pass is dated from its start and turned by `rotate`
//    round its length: position i holds what was played at i - rotate;
//  - notes are cut at the end of their pass and of the turned loop.

#include <Recording/Takes/Passes.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <dr_wav.h>

#include <cmath>

using namespace Recording;

TEST_CASE("Passes are counted from the start, complete ones only", "[recording][passes]")
{
  auto p = passesOf(100, 1050, 300);
  REQUIRE(p.size() == 3);
  CHECK(p[0].start == 100);
  CHECK(p[2].end == 1000);
  // Shorter than a pass: itself
  auto one = passesOf(100, 250, 300);
  REQUIRE(one.size() == 1);
  CHECK(one[0].end == 250);
  CHECK(passesOf(100, 100, 300).empty());
}

TEST_CASE("The notes of a pass, turned round it", "[recording][passes]")
{
  std::vector<RecordedNote> notes{
      {110, 20, 60, 100, 0},  // pass 0
      {390, 50, 61, 100, 0},  // pass 1, crossing its end
      {420, 10, 62, 100, 0}}; // pass 2
  // Pass [100, 400): the notes starting in it; the one at 390 is cut at 400
  auto a = notesOfPass(notes, {100, 400}, 0);
  REQUIRE(a.size() == 2);
  CHECK(a[0].start == 10);
  CHECK(a[1].start == 290);
  CHECK(a[1].duration == 10);

  // Pass [400, 700): the note at 420
  auto c = notesOfPass(notes, {400, 700}, 0);
  REQUIRE(c.size() == 1);
  CHECK(c[0].pitch == 62);
  CHECK(c[0].start == 20);

  // Turned by 290: 20 + 290 = 310 -> 10
  auto d = notesOfPass(notes, {400, 700}, 290);
  REQUIRE(d.size() == 1);
  CHECK(d[0].start == 10);
  CHECK(d[0].duration == 10);

  // Turned so that it would cross the turned loop's end: cut there
  auto e = notesOfPass({{110, 50, 60, 100, 0}}, {100, 400}, 270);
  REQUIRE(e.size() == 1);
  CHECK(e[0].start == 280);
  CHECK(e[0].duration == 20);
}

TEST_CASE("The values of a pass, turned round it", "[recording][passes]")
{
  std::vector<ValueTake::Event> ev{{120, 1.f}, {250, 2.f}, {390, 3.f}, {410, 4.f}};
  auto a = valuesOfPass(ev, {100, 400}, 100);
  REQUIRE(a.size() == 3);
  // 120 -> 20 + 100 = 120; 250 -> 250; 390 -> 290 + 100 = 390 -> 90
  CHECK(a[0].date == 90);
  CHECK(a[0].value == ossia::value{3.f});
  CHECK(a[1].date == 120);
  CHECK(a[2].date == 250);
}

TEST_CASE("A pass of a WAV file is written turned", "[recording][passes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString src = dir.filePath("src.wav");
  {
    drwav_data_format fmt{};
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    fmt.channels = 2;
    fmt.sampleRate = 48000;
    fmt.bitsPerSample = 32;
    drwav w;
    REQUIRE(drwav_init_file_write(&w, src.toStdString().c_str(), &fmt, nullptr));
    std::vector<float> data(2 * 1000);
    for(int i = 0; i < 1000; i++)
    {
      data[2 * i] = float(i);
      data[2 * i + 1] = float(-i);
    }
    drwav_write_pcm_frames(&w, 1000, data.data());
    drwav_uninit(&w);
  }
  const QString dst = dir.filePath("pass.wav");
  // Frames 200 to 500, turned by 100
  REQUIRE(writeWavSegment(src, dst, 200, 300, 100, WavStreamWriter::Format::Float32));
  drwav r;
  REQUIRE(drwav_init_file(&r, dst.toStdString().c_str(), nullptr));
  REQUIRE(r.channels == 2);
  REQUIRE(r.totalPCMFrameCount == 300);
  std::vector<float> out(600);
  drwav_read_pcm_frames_f32(&r, 300, out.data());
  drwav_uninit(&r);
  // i holds what was at i - 100 of the segment: 0 -> 200 + 200 = 400
  CHECK(out[0] == 400.f);
  CHECK(out[1] == -400.f);
  CHECK(out[2 * 99] == 499.f);
  CHECK(out[2 * 100] == 200.f);
  CHECK(out[2 * 299] == 399.f);

  // Past the end of the source: silence
  REQUIRE(writeWavSegment(src, dst, 900, 200, 0, WavStreamWriter::Format::Float32));
  REQUIRE(drwav_init_file(&r, dst.toStdString().c_str(), nullptr));
  std::vector<float> out2(400);
  drwav_read_pcm_frames_f32(&r, 200, out2.data());
  drwav_uninit(&r);
  CHECK(out2[2 * 99] == 999.f);
  CHECK(out2[2 * 100] == 0.f);
}
