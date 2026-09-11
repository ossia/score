// The waveform a soundfile control draws beside itself.
//
// score::QGraphicsWaveformButton asked its AudioFile to tell it when decoding
// finished, and computed the waveform from that. On loading a document the
// file is usually decoded already -- the manager caches it, and a sound
// process elsewhere in the score often pulled it in first -- so the signal
// never came and the control stayed on its "drop a sound file here"
// placeholder until the file was picked again by hand.

#include <Media/AudioFileChooserWidget.hpp>
#include <Media/MediaFileHandle.hpp>

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGraphicsScene>
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
}

TEST_CASE("a soundfile control shows the waveform of an already-decoded file")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    // Someone else has the file open already: exactly the state a document
    // that has just been loaded is in.
    auto held = Media::AudioFileManager::instance().get(wav, 0);
    REQUIRE(held);
    REQUIRE(spin_until([&] { return held->finishedDecoding(); }));

    QGraphicsScene scene;
    score::QGraphicsWaveformButton bt{nullptr};
    scene.addItem(&bt);

    CHECK_FALSE(bt.hasWaveform());
    bt.setFile(wav, wav);
    CHECK(bt.text() == wav);

    // The computation itself is threaded; what matters is that it was asked
    // for at all.
    CHECK(spin_until([&] { return bt.hasWaveform(); }));
  });
}

TEST_CASE("a soundfile control forgets the waveform when the file is cleared")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    auto held = Media::AudioFileManager::instance().get(wav, 0);
    REQUIRE(held);
    REQUIRE(spin_until([&] { return held->finishedDecoding(); }));

    QGraphicsScene scene;
    score::QGraphicsWaveformButton bt{nullptr};
    scene.addItem(&bt);

    bt.setFile(wav, wav);
    REQUIRE(spin_until([&] { return bt.hasWaveform(); }));

    bt.setFile({}, {});
    CHECK_FALSE(bt.hasWaveform());
  });
}


// What the document stores is not always what can be opened. A sound picked
// from next to the .score file is saved as "<PROJECT>:name.wav", and the audio
// file manager's two-argument get() opens the path it is handed -- so the
// widget has to be given the resolved one as well as the stored one. Handing
// it only the stored form left audiofx.score's Granola with no waveform and no
// error anywhere.
TEST_CASE("the waveform button decodes the resolved path, not the stored one")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = dir.path() + "/tone.wav";
    score::test::write_wav(wav, 1.0);

    QGraphicsScene scene;
    score::QGraphicsWaveformButton bt{nullptr};
    scene.addItem(&bt);

    // The stored form, of the shape a document holds, with the resolved path
    // beside it.
    const QString stored = QStringLiteral("<PROJECT>:") + QFileInfo{wav}.fileName();
    bt.setFile(stored, wav);

    // The label keeps what the document said...
    CHECK(bt.text() == stored);
    // ... and the decode used the path that exists.
    REQUIRE(spin_until([&] { return bt.hasWaveform(); }));
  });
}
