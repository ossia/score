// SCORE_AUDIO_NO_INPUT=1 defaults the audio settings to no input card while a
// stored card still wins. score::setupDefaultSettings writes every missing key
// with its default before the environment is looked at, so the default has to
// be recognised by its value (an empty card), not by a missing key.

#include <Audio/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
struct NoInputEnv
{
  NoInputEnv() { qputenv("SCORE_AUDIO_NO_INPUT", "1"); }
  ~NoInputEnv() { qunsetenv("SCORE_AUDIO_NO_INPUT"); }
};
}

TEST_CASE("SCORE_AUDIO_NO_INPUT defaults to no input card", "[audio][settings]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    NoInputEnv env;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("settings.ini");
    const UuidKey<score::SettingsDelegateFactory> key{};

    SECTION("a fresh settings file")
    {
      QSettings set{path, QSettings::IniFormat};
      Audio::Settings::Model m{key, set, ctx};
      CHECK(m.getCardIn() == "No device");
      CHECK(m.getDefaultIn() == 0);
      CHECK(m.getCardOut().isEmpty());

      // The defaults were written: the next start is still unconfigured.
      Audio::Settings::Model again{key, set, ctx};
      CHECK(again.getCardIn() == "No device");
      CHECK(again.getDefaultIn() == 0);
    }

    SECTION("a stored card wins")
    {
      {
        QSettings set{path, QSettings::IniFormat};
        set.setValue("Audio/CardIn", "my card");
        set.setValue("Audio/DefaultIn", 4);
      }
      QSettings set{path, QSettings::IniFormat};
      Audio::Settings::Model m{key, set, ctx};
      CHECK(m.getCardIn() == "my card");
      CHECK(m.getDefaultIn() == 4);
    }
  });
}
