// score's audio engine (Audio::ApplicationPlugin) on real CoreAudio devices.
//
// Hardware-gated like CoreAudioEngineTest: needs macOS with BlackHole, skipped
// elsewhere. The devices used are private aggregates of this process, and the
// settings are the test application's own: the machine is left as it was.

#include "CoreAudioTestUtils.hpp"

#if defined(__APPLE__) && OSSIA_ENABLE_MINIAUDIO
#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/audio_engine.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QTimer>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

using namespace coreaudio_test;

namespace
{
const Audio::AudioFactory::ConcreteKey coreaudio_driver{
    score::uuids::string_generator::compute("85115103-694a-4a3b-9274-76ef47aec5a9")};
const Audio::AudioFactory::ConcreteKey dummy_driver{
    score::uuids::string_generator::compute("13dabcc3-9cda-422f-a8c7-5fef5c220677")};

//! What selecting the CoreAudio driver does (also at startup): the factory
//! matches the stored cards against the devices it knows.
void reselect_coreaudio(Audio::Settings::Model& set)
{
  set.setDriver(dummy_driver);
  set.setDriver(coreaudio_driver);
}

//! Processes events until `pred` holds or `ms` elapsed.
template <typename F>
bool spin_until(F&& pred, int ms)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  return pred();
}

//! The app fixture forces the dummy backend unless told otherwise
void run_with_coreaudio(auto&& f)
{
  // The test application's settings persist across runs: a card that a test
  // left there, gone since, would be opened at startup (and fail, loudly).
  // Its preferences domain, as the fixture names the application.
  {
    const auto app = qEnvironmentVariableIsSet("SCORE_CUSTOM_APP_APPLICATION_NAME")
                         ? qEnvironmentVariable("SCORE_CUSTOM_APP_APPLICATION_NAME")
                         : QStringLiteral("score-test");
    const QString domain = QStringLiteral("io.ossia.") + app;
    CFStringRef cfdomain = domain.toCFString();
    for(auto key : {"Audio.CardIn", "Audio.CardOut", "Audio.DefaultIn", "Audio.DefaultOut",
                    "Audio.Rate", "Audio.BufferSize"})
    {
      CFStringRef cfkey = CFStringCreateWithCString(nullptr, key, kCFStringEncodingUTF8);
      CFPreferencesSetAppValue(cfkey, nullptr, cfdomain);
      CFRelease(cfkey);
    }
    CFPreferencesAppSynchronize(cfdomain);
    CFRelease(cfdomain);
  }
  qputenv("SCORE_AUDIO_BACKEND", "coreaudio");
  if(!qEnvironmentVariableIsSet("SCORE_DISABLE_AUDIOPLUGINS"))
    qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  AllNominalRatesGuard rates;
  score::test::run_in_app(f);
}

//! Answers the modal message boxes as a user would, and remembers them
struct MessageBoxes
{
  QStringList texts;
  QTimer timer;
  MessageBoxes()
  {
    QObject::connect(&timer, &QTimer::timeout, [this] {
      if(auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      {
        texts.push_back(box->text());
        box->accept();
      }
    });
    timer.start(20);
  }
};

Audio::ApplicationPlugin& audioPlugin(const score::GUIApplicationContext& ctx)
{
  return ctx.guiApplicationPlugin<Audio::ApplicationPlugin>();
}

//! Takes score off the test's devices before they go away (declared after
//! them, so destroyed first, also when a REQUIRE gave up): the engine moves to
//! the dummy driver, and no card is left in the settings, which persist.
struct ReleaseAudio
{
  const score::GUIApplicationContext& ctx;
  ~ReleaseAudio()
  {
    MessageBoxes boxes;
    auto& set = ctx.settings<Audio::Settings::Model>();
    set.setCardIn("");
    set.setCardOut("");
    set.setDriver(dummy_driver);
    set.changed();
    spin_until([] { return false; }, 300);
  }
};

//! Points the audio settings at `uid`, both ways, and restarts the engine.
void use_device(
    const score::GUIApplicationContext& ctx, const std::string& uid, int chans, int bs,
    int rate = 48000)
{
  auto& set = ctx.settings<Audio::Settings::Model>();
  set.setDriver(coreaudio_driver);
  set.setCardIn(QString::fromStdString(uid));
  set.setCardOut(QString::fromStdString(uid));
  set.setDefaultIn(chans);
  set.setDefaultOut(chans);
  set.setRate(rate);
  set.setBufferSize(bs);
  set.changed();
}
}

TEST_CASE("score stops the engine whose device went away", "[coreaudio][hardware][app]")
{
  if(!has_blackhole())
    SKIP("BlackHole not installed");

  run_with_coreaudio([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& audio = audioPlugin(ctx);

    PrivateAggregate agg{"ossia-test-app-lost", {uid_of("BlackHole 16ch")}};
    ReleaseAudio release{ctx};
    const auto before = audio.audio;
    use_device(ctx, "io.ossia.test.ossia-test-app-lost", 16, 256);
    // The restart is queued: wait for the engine it makes
    REQUIRE(spin_until(
        [&] { return audio.audio && audio.audio != before && audio.audio->running(); },
        3000));
    REQUIRE(audio.audio->effective_outputs == 16);

    // Playing when the device goes
    ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution().request_play_global(true);
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    REQUIRE(spin_until(
        [&] {
      return exec.isPlaying()
             && Audio::execution_status.load() == ossia::transport_status::playing;
    }, 2000));

    MessageBoxes boxes;
    agg.destroy();

    // The engine notices, is stopped, the user is told; the playback stops
    CHECK(spin_until([&] { return !audio.audio; }, 1500));
    CHECK(!exec.isPlaying());
    CHECK(spin_until([&] { return !boxes.texts.empty(); }, 1500));
    REQUIRE(boxes.texts.size() == 1);
    CHECK(boxes.texts[0].contains("stopped working"));
  });
}
TEST_CASE("score runs at the rate the device does", "[coreaudio][hardware][app][rate]")
{
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  NominalRateGuard bh16{"BlackHole 16ch"};
  REQUIRE(set_nominal_rate(bh16.dev, 48000));

  run_with_coreaudio([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& audio = audioPlugin(ctx);
    auto& set = ctx.settings<Audio::Settings::Model>();
    MessageBoxes boxes;
    ReleaseAudio release{ctx};

    // A rate the device cannot do: score adopts the device's
    auto before = audio.audio;
    use_device(ctx, uid_of("BlackHole 16ch"), 16, 256, 22050);
    REQUIRE(spin_until(
        [&] { return audio.audio && audio.audio != before && audio.audio->running(); },
        3000));
    CHECK(audio.audio->effective_sample_rate == 48000);
    CHECK(set.getRate() == 48000);

    // The device's rate changed by someone else: score restarts at it
    before = audio.audio;
    REQUIRE(set_nominal_rate(bh16.dev, 96000));
    CHECK(spin_until(
        [&] {
      return audio.audio && audio.audio != before && audio.audio->running()
             && audio.audio->effective_sample_rate == 96000;
    }, 3000));
    CHECK(set.getRate() == 96000);
  });
}

TEST_CASE("score uses the buffer size the device does", "[coreaudio][hardware][app][buffer]")
{
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  run_with_coreaudio([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& audio = audioPlugin(ctx);
    auto& set = ctx.settings<Audio::Settings::Model>();
    MessageBoxes boxes;
    ReleaseAudio release{ctx};

    // BlackHole's maximum is 4096 frames
    auto before = audio.audio;
    use_device(ctx, uid_of("BlackHole 16ch"), 16, 8192);
    REQUIRE(spin_until(
        [&] { return audio.audio && audio.audio != before && audio.audio->running(); },
        3000));
    CHECK(audio.audio->effective_buffer_size == 4096);
    CHECK(set.getBufferSize() == 4096);
  });
}

TEST_CASE("A device that is not there is not replaced by another", "[coreaudio][hardware][app][devices]")
{
  // Starting score while the card is unplugged replaced it, for good, in the
  // settings: by the default device, which then played what was meant for
  // the card.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  run_with_coreaudio([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& audio = audioPlugin(ctx);
    auto& set = ctx.settings<Audio::Settings::Model>();
    MessageBoxes boxes;
    ReleaseAudio release{ctx};

    const QString missing = "io.ossia.test.not-plugged-in";
    set.setCardIn(missing);
    set.setCardOut(missing);
    set.setDefaultIn(24);
    set.setDefaultOut(24);
    reselect_coreaudio(set);
    CHECK(set.getCardIn() == missing);
    CHECK(set.getCardOut() == missing);
    CHECK(set.getDefaultOut() == 24);

    // No engine plays on another device; the user is told
    spin_until([&] { return !boxes.texts.empty(); }, 2000);
    CHECK((!audio.audio || !audio.audio->running()));
    CHECK(!boxes.texts.empty());
  });
}

TEST_CASE("A device plugged in while score runs can be chosen", "[coreaudio][hardware][app][devices]")
{
  // The devices were listed once, when score started.
  if(!has_blackhole())
    SKIP("BlackHole not installed");
  run_with_coreaudio([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& set = ctx.settings<Audio::Settings::Model>();
    MessageBoxes boxes;
    reselect_coreaudio(set);

    PrivateAggregate agg{"ossia-test-plugged", {uid_of("BlackHole 16ch"), uid_of("BlackHole 64ch")}};
    ReleaseAudio release{ctx};
    // The HAL announces the new device asynchronously
    spin_until([] { return false; }, 300);

    set.setCardIn(QString::fromStdString("io.ossia.test.ossia-test-plugged"));
    set.setCardOut(QString::fromStdString("io.ossia.test.ossia-test-plugged"));
    set.setDefaultIn(2);
    set.setDefaultOut(2);
    reselect_coreaudio(set);
    // Known: its channels are taken from it
    CHECK(set.getDefaultIn() == 80);
    CHECK(set.getDefaultOut() == 80);
  });
}

#endif
