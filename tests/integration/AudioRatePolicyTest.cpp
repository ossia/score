// The rate and buffer size the device runs at are used for the session; the
// user's own setting stays saved, and is asked for again at the next start.
// Only choosing a value saves it.

#include <score_test/App.hpp>

#include <Audio/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QSettings>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "a rate adopted from the device is not saved, a rate chosen is",
    "[audio][settings]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& set = ctx.settings<Audio::Settings::Model>();
    const int rate = set.getRate(), bs = set.getBufferSize();
    const auto saved = [](const char* key) { return QSettings{}.value(key).toInt(); };

    set.setRate(48000);
    set.setBufferSize(512);
    CHECK(set.savedRate() == 48000);
    CHECK(saved("Audio/SamplingRate") == 48000);
    CHECK(saved("Audio/BufferSize") == 512);

    // What the device runs at: used, not saved
    int rateSignals = 0;
    auto c = QObject::connect(
        &set, &Audio::Settings::Model::RateChanged, [&](int) { rateSignals++; });
    set.adoptRate(44100);
    set.adoptBufferSize(256);
    CHECK(set.getRate() == 44100);
    CHECK(set.getBufferSize() == 256);
    CHECK(rateSignals == 1);
    CHECK(set.savedRate() == 48000);
    CHECK(saved("Audio/SamplingRate") == 48000);
    CHECK(saved("Audio/BufferSize") == 512);

    // Choosing the value it already runs at saves it
    set.setRate(44100);
    CHECK(rateSignals == 1);
    CHECK(set.savedRate() == 44100);
    CHECK(saved("Audio/SamplingRate") == 44100);
    QObject::disconnect(c);

    set.setRate(rate);
    set.setBufferSize(bs);
  });
}
