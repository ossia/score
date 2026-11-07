// The waveform of a sound file, as widgets draw it: the stock soundfile
// control, and custom avendish items through draw_waveform().

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/WaveformRenderer.hpp>

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

namespace
{
//! Pumps the event loop until `f` holds, or gives up.
template <typename F>
bool spin_until(F f, int ms = 5000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

//! Whether some pixel of the image is not the background colour.
bool has_ink(const QImage& img, QColor bg)
{
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      if(img.pixelColor(x, y) != bg)
        return true;
  return false;
}
}

TEST_CASE("the waveform renderer draws a decoded file, on its own thread")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    Media::Sound::WaveformRenderer r{QSizeF{200., 60.}};
    int updates = 0;
    QObject::connect(
        &r, &Media::Sound::WaveformRenderer::updated, &r, [&] { updates++; });

    CHECK_FALSE(r.hasWaveform());
    r.setFile(wav);
    CHECK(r.path() == wav);
    REQUIRE(spin_until([&] { return r.hasWaveform(); }));
    CHECK(updates > 0);

    QImage img{200, 60, QImage::Format_ARGB32_Premultiplied};
    img.fill(Qt::black);
    {
      QPainter p{&img};
      r.paint(p, QRectF{0., 0., 200., 60.}, 1.);
    }
    CHECK(has_ink(img, Qt::black));
  });
}

// A zoomed widget asks for part of the file: the images are made for that
// window of it, and the renderer keeps drawing what it has until they come.
TEST_CASE("the waveform renderer draws a part of the file")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    Media::Sound::WaveformRenderer r{QSizeF{200., 60.}};
    r.setFile(wav);
    REQUIRE(spin_until([&] { return r.hasWaveform(); }));

    int updates = 0;
    QObject::connect(
        &r, &Media::Sound::WaveformRenderer::updated, &r, [&] { updates++; });
    r.setRange(0.25, 0.75);
    CHECK(r.hasWaveform());
    REQUIRE(spin_until([&] { return updates > 0; }));
    CHECK(r.hasWaveform());

    QImage img{200, 60, QImage::Format_ARGB32_Premultiplied};
    img.fill(Qt::black);
    {
      QPainter p{&img};
      r.paint(p, QRectF{0., 0., 200., 60.}, 1.);
    }
    CHECK(has_ink(img, Qt::black));
  });
}

TEST_CASE("the waveform renderer forgets the waveform when the file is cleared")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    Media::Sound::WaveformRenderer r{QSizeF{200., 60.}};
    r.setFile(wav);
    REQUIRE(spin_until([&] { return r.hasWaveform(); }));

    r.setFile({});
    CHECK_FALSE(r.hasWaveform());
    CHECK_FALSE(r.file());
  });
}
