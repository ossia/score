// The package manager used to tell the user that packages were out of date
// with a modal box, in front of whatever they were doing and away from the
// packages themselves. The message now goes on the process library panel,
// which is where those packages appear, and its link opens the settings page
// that installs them.
//
// This covers the panel side: the notice reaches the panel's widget tree, it
// is only shown while there is something to say, and activating its link runs
// what the caller handed over.

#include <score_test/App.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/settings/SettingsView.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <QApplication>
#include <QLabel>
#include <QListWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
//! The notice is private to ProcessWidget, so it is found the way the user
//! sees it: a label in the panel's own widget tree carrying that text.
QLabel* notice_of(QWidget& w, const QString& text)
{
  for(auto* l : w.findChildren<QLabel*>())
    if(l->text() == text)
      return l;
  return nullptr;
}
}

TEST_CASE(
    "an out-of-date package is announced on the process library panel",
    "[integration][library][packages][gui]")
{
  score::test::prepare_test_environment(/*headless=*/false);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  auto* panel = app.context().findPanel<Library::ProcessPanel>();
  REQUIRE(panel != nullptr);

  auto& widget = panel->processWidget();

  const QString text
      = QStringLiteral("Some package can be updated. <a href=\"packages\">Update</a>");

  // Nothing to say yet: nothing is shown.
  CHECK(notice_of(widget, text) == nullptr);

  int activated = 0;
  widget.setNotice(text, [&] { activated++; });
  QApplication::processEvents();

  auto* label = notice_of(widget, text);
  REQUIRE(label != nullptr);
  CHECK(!label->isHidden());

  // The link does what the caller asked, rather than opening a browser.
  CHECK(label->openExternalLinks() == false);
  Q_EMIT label->linkActivated(QStringLiteral("packages"));
  CHECK(activated == 1);

  // Everything up to date again: the line goes away.
  widget.setNotice({});
  QApplication::processEvents();
  CHECK(label->isHidden());
  CHECK(label->text().isEmpty());
}

namespace
{
//! The smallest thing SettingsView::addSettingsView will take: it asks a page
//! for its name, its icon and its widget, and nothing else.
struct FakePage final
{
  struct View final : score::SettingsDelegateView<score::SettingsDelegateModel>
  {
    QWidget* getWidget() override { return &w; }
    QWidget w;
  };

  struct Presenter final
      : score::SettingsDelegatePresenter<score::SettingsDelegateModel>
  {
    using score::SettingsDelegatePresenter<
        score::SettingsDelegateModel>::SettingsDelegatePresenter;
    QString settingsName() override { return name; }
    QIcon settingsIcon() override { return {}; }
    QString name;
  };

  explicit FakePage(QString n)
      : model{UuidKey<score::SettingsDelegateFactory>{}, nullptr}
      , presenter{model, view, nullptr}
  {
    presenter.name = std::move(n);
    view.setPresenter(&presenter);
  }

  score::SettingsDelegateModel model;
  View view;
  Presenter presenter;
};
}

TEST_CASE(
    "the settings dialog can be opened straight on a named page",
    "[integration][settings][gui]")
{
  score::test::prepare_test_environment(/*headless=*/false);

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  QApplication app{argc, argv};

  score::SettingsView<score::SettingsDelegateModel> view{nullptr};

  FakePage general{QStringLiteral("General")};
  FakePage packages{QStringLiteral("Packages")};
  FakePage audio{QStringLiteral("Audio")};
  view.addSettingsView(&general.view);
  view.addSettingsView(&packages.view);
  view.addSettingsView(&audio.view);

  auto* list = view.findChild<QListWidget*>();
  REQUIRE(list != nullptr);

  CHECK(view.setCurrentSettings(QStringLiteral("Packages")));
  REQUIRE(list->currentItem() != nullptr);
  CHECK(list->currentItem()->text() == QStringLiteral("Packages"));

  // A name nobody goes by leaves the dialog on the page it was on, rather than
  // on some arbitrary one.
  CHECK(!view.setCurrentSettings(QStringLiteral("Nonexistent")));
  CHECK(list->currentItem()->text() == QStringLiteral("Packages"));
}
