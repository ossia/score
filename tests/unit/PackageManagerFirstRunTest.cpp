// PM::PluginSettingsModel::firstTimeLibraryDownload() asks, with a modal
// score::question(), whether to download the user library when it is missing.
// It is posted by a 1s QTimer from on_message(), so ANY process that pumps the
// event loop for more than a second gets the dialog -- including a headless
// one, where QDialog::exec() spins forever on an event loop nobody can click
// on.
//
// Called directly here rather than waited for: the question is which branch the
// function takes when there is no interactive GUI, and that is the same
// decision whichever timer delivered it.

#include <Library/LibrarySettings.hpp>
#include <PackageManager/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <score_test/App.hpp>

#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the first-run library question never fires headless", "[packagemanager]")
{
  bool returned = false;

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    // run_in_app boots on the offscreen QPA: no window manager, nobody to
    // answer. The fixture sets SCORE_SANITIZE_SKIP_CHECKS for every test it
    // starts, so drop it here: what has to keep the dialog shut is the platform
    // check, not the escape hatch. interactiveSession() reads the environment
    // when firstTimeLibraryDownload() asks, so removing it now is in time.
    qunsetenv("SCORE_SANITIZE_SKIP_CHECKS");
    REQUIRE_FALSE(qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"));
    REQUIRE(QGuiApplication::platformName() == QLatin1String("offscreen"));

    auto& lib = ctx.settings<Library::Settings::Model>();
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    lib.setRootPath(tmp.path());

    // The branch that opens the dialog is "the library is not there yet".
    REQUIRE_FALSE(QFile::exists(lib.getPackagesPath() + "/default/package.json"));

    ctx.settings<PM::PluginSettingsModel>().firstTimeLibraryDownload();
    returned = true;
  });

  CHECK(returned);
}
