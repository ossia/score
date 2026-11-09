// The start screen tells whether the default library is missing or has an
// update, installs it through the package manager's installer, and lists a
// downloaded online example on the online page too. No network: the example
// manifest and files are on disk, and the package list entry, whose archive is
// a file:// URL, is handed to the package manager directly.

#include <Library/LibrarySettings.hpp>
#include <PackageManager/Model.hpp>
#include <PackageManager/PluginItemModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/Zip.hpp>

#include <core/document/DefaultLibrary.hpp>

#include <score_test/App.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "StartScreen.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::StartScreen)
W_OBJECT_IMPL(score::InteractiveLabel)

namespace
{
bool waitFor(const std::function<bool()>& pred, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  return pred();
}

void writeFile(const QString& path, const QByteArray& data)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(data);
}

void writePackageJson(const QString& library, int version)
{
  QJsonObject obj;
  obj["name"] = "Default library";
  obj["raw_name"] = "default";
  obj["key"] = score::DefaultLibrary::key().toString(QUuid::WithoutBraces);
  obj["version"] = version;
  writeFile(library + "/packages/default/package.json", QJsonDocument{obj}.toJson());
}

//! A zip at `archive` holding `files`, as {name in the archive, content}.
void writeZip(
    const QString& archive, const std::vector<std::pair<QString, QByteArray>>& files)
{
  QTemporaryDir src;
  REQUIRE(src.isValid());
  std::vector<score::ZipEntry> entries;
  int i = 0;
  for(const auto& [name, content] : files)
  {
    const auto path = src.filePath(QString::number(i++));
    writeFile(path, content);
    entries.push_back({path, name});
  }
  QDir{}.mkpath(QFileInfo{archive}.absolutePath());
  QString error;
  REQUIRE(score::writeZipArchive(archive, entries, 0, error));
}

//! The default library's entry in the package list, downloaded from `url`.
QJsonObject defaultLibraryEntry(const QUrl& url, int version)
{
  return QJsonObject{
      {"name", "Default library"},
      {"raw_name", "default"},
      {"kind", "media"},
      {"key", "c0ef5539-774c-477c-9989-234da83f284f"},
      {"file", url.toString()},
      {"version", version},
      {"size", "1K"}};
}

//! The entries of `screen` the user can see, with the given text.
std::vector<score::InteractiveLabel*>
visibleItems(QWidget& screen, const std::function<bool(const QString&)>& text)
{
  std::vector<score::InteractiveLabel*> out;
  for(auto* l : screen.findChildren<score::InteractiveLabel*>())
    if(l->isVisibleTo(&screen) && text(l->text()))
      out.push_back(l);
  return out;
}

std::vector<score::InteractiveLabel*> visibleItems(QWidget& screen, const QString& text)
{
  return visibleItems(screen, [&](const QString& t) { return t == text; });
}

bool hintShown(QWidget& screen, const QString& text)
{
  for(auto* l : screen.findChildren<QLabel*>())
    if(l->isVisibleTo(&screen) && l->text().startsWith(text))
      return true;
  return false;
}

void showPage(QWidget& screen, const QString& name)
{
  auto nav = visibleItems(screen, name);
  REQUIRE(nav.size() == 1);
  nav.front()->labelPressed({});
}

std::vector<score::ExampleCard*> visibleCards(QWidget& screen)
{
  std::vector<score::ExampleCard*> out;
  for(auto* w : screen.findChildren<QWidget*>())
    if(auto card = dynamic_cast<score::ExampleCard*>(w); card && card->isVisibleTo(&screen))
      out.push_back(card);
  return out;
}

//! An empty user library, in the settings the start screen reads.
struct UserLibrary
{
  QTemporaryDir dir;
  QString root;
  QString previous;

  UserLibrary()
  {
    REQUIRE(dir.isValid());
    root = dir.filePath("library");
    QDir{}.mkpath(root);
    QSettings s;
    previous = s.value("Library/RootPath").toString();
    s.setValue("Library/RootPath", root);
    s.remove("score-docs");

    // The start screen fetches the example list: from nowhere, unless the test
    // gave one.
    if(!qEnvironmentVariableIsSet("SCORE_EXAMPLES_MANIFEST"))
      qputenv(
          "SCORE_EXAMPLES_MANIFEST",
          QUrl::fromLocalFile(dir.filePath("no-manifest.json")).toString().toUtf8());
  }
  ~UserLibrary() { QSettings{}.setValue("Library/RootPath", previous); }
};

//! The visible menu of `screen`, null when none is open.
QMenu* openMenu(QWidget& screen)
{
  for(auto* m : screen.findChildren<QMenu*>())
    if(m->isVisible())
      return m;
  return nullptr;
}

QAction* menuAction(QMenu& menu, const QString& text)
{
  for(auto* a : menu.actions())
    if(a->text() == text)
      return a;
  return nullptr;
}

struct Screen
{
  QWidget window;
  score::StartScreen* screen{};
  Screen()
  {
    window.resize(score::StartScreen::Width, score::StartScreen::Height);
    screen = new score::StartScreen{{}, &window};
    window.show();
    QApplication::processEvents();
  }
};
}

TEST_CASE("the start screen offers to install a missing default library", "[startscreen]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    UserLibrary lib;
    auto& defaultLibrary = score::DefaultLibrary::instance();
    REQUIRE(defaultLibrary.state() == score::DefaultLibrary::State::Missing);

    // What the package manager's installer does, without the download.
    int installs = 0;
    defaultLibrary.setInstaller([&] {
      installs++;
      writePackageJson(lib.root, 1);
      writeFile(lib.root + "/packages/default/Templates/Installation.score", "{}");
      defaultLibrary.setFinished(true);
    });

    Screen s;
    CHECK(defaultLibrary.offeredInView());
    CHECK(hintShown(*s.screen, "The default library is not installed. It provides the templates"));

    // Built before the install, so that it has to be rebuilt to show them.
    showPage(*s.screen, "Templates");
    CHECK(visibleCards(*s.screen).empty());

    auto install = visibleItems(*s.screen, [](const QString& t) {
      return t.startsWith("Install the default library");
    });
    REQUIRE(install.size() == 1);
    install.front()->labelPressed({});
    CHECK(installs == 1);
    CHECK(defaultLibrary.state() == score::DefaultLibrary::State::Current);

    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == 1; }));
    CHECK(visibleCards(*s.screen).front()->path().endsWith("Installation.score"));
    CHECK(hintShown(*s.screen, "The default library is installed."));
    CHECK(visibleItems(*s.screen, "Show the templates").empty());

    showPage(*s.screen, "Home");
    CHECK(visibleItems(*s.screen, "Show the templates").size() == 1);
  });
}

TEST_CASE("the start screen tells when the default library has an update", "[startscreen]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    UserLibrary lib;
    writePackageJson(lib.root, 3);
    auto& defaultLibrary = score::DefaultLibrary::instance();
    defaultLibrary.setAvailable(3, 0);
    REQUIRE(defaultLibrary.state() == score::DefaultLibrary::State::Current);

    Screen s;
    // Installed and current: nothing about it.
    CHECK_FALSE(hintShown(*s.screen, "The default library"));
    CHECK_FALSE(hintShown(*s.screen, "A new version of the default library"));

    defaultLibrary.setAvailable(4, 80 * 1024 * 1024);
    CHECK(hintShown(*s.screen, "A new version of the default library is available"));
    CHECK(visibleItems(*s.screen, "Update the default library (80.00 MiB)").size() == 1);

    // Progress, then a failure, as the package manager reports them.
    defaultLibrary.setProgress(1, 4);
    CHECK(hintShown(*s.screen, "Downloading the default library: 25%"));
    CHECK(visibleItems(*s.screen, [](const QString& t) {
      return t.contains("default library");
    }).empty());

    defaultLibrary.setFinished(false, "Host not found");
    CHECK(hintShown(*s.screen, "The default library could not be installed: Host not found"));
    CHECK(visibleItems(*s.screen, "Try again").size() == 1);

    defaultLibrary.setFinished(true);
    defaultLibrary.setAvailable(-1, 0);
  });
}

TEST_CASE("the package list's default library becomes the start screen's installer", "[startscreen]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& pm = ctx.settings<PM::PluginSettingsModel>();
    pm.handleAddon(QJsonDocument::fromJson(R"({
        "name": "Default library",
        "raw_name": "default",
        "kind": "media",
        "key": "c0ef5539-774c-477c-9989-234da83f284f",
        "file": "https://github.com/ossia/score-user-library/archive/master.zip",
        "version": 12,
        "size": "80M"})").object());

    auto& defaultLibrary = score::DefaultLibrary::instance();
    CHECK(defaultLibrary.availableVersion() == 12);
    CHECK(defaultLibrary.downloadSize() == 80 * 1024 * 1024);
    CHECK(defaultLibrary.canInstall());

    // Another package is not the default library.
    pm.handleAddon(QJsonDocument::fromJson(R"({
        "name": "Drum kits",
        "raw_name": "drum-kits",
        "kind": "media",
        "key": "1d2ec7b8-5e44-4f0e-8a5b-3a6a52f8f1c9",
        "file": "https://example.com/drums.zip",
        "version": 40,
        "size": "1M"})").object());
    CHECK(defaultLibrary.availableVersion() == 12);
    defaultLibrary.setAvailable(-1, 0);
  });
}

TEST_CASE("a downloaded online example stays on the online page", "[startscreen]")
{
  QTemporaryDir site;
  REQUIRE(site.isValid());
  const auto file = site.filePath("looper.score");
  writeFile(file, "{}");
  writeFile(
      site.filePath("index.json"),
      QStringLiteral(R"({"version":1,"examples":[
{"id":"examples/audio/looper","name":"Looper","section":"examples","group":"audio",
 "file":"%1","format":"score","size":2,"platforms":[]},
{"id":"examples/audio/granular","name":"Granular","section":"examples","group":"audio",
 "file":"%1","format":"score","size":2,"platforms":[]}]})")
          .arg(QUrl::fromLocalFile(file).toString())
          .toUtf8());
  qputenv(
      "SCORE_EXAMPLES_MANIFEST",
      QUrl::fromLocalFile(site.filePath("index.json")).toString().toUtf8());
  qputenv("XDG_CACHE_HOME", site.filePath("cache").toUtf8());

  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    UserLibrary lib;
    // The copy of the looper, and the size the site had when it was downloaded.
    writeFile(lib.root + "/Examples/score-docs/examples/audio/looper.score", "{}");
    QSettings{}.setValue("score-docs/downloaded/examples/audio/looper", 1);

    Screen s;
    showPage(*s.screen, "Online examples");
    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == 2; }));

    int copies = 0;
    for(auto* card : visibleCards(*s.screen))
    {
      if(card->path().isEmpty())
      {
        CHECK(card->badge() != "Updated online");
        continue;
      }
      copies++;
      CHECK(card->path().endsWith("Examples/score-docs/examples/audio/looper.score"));
      CHECK(card->badge() == "Updated online");
      // The way to get the new version
      CHECK(card->toolTip().contains("Right-click to download it again"));
    }
    CHECK(copies == 1);

    showPage(*s.screen, "Local examples");
    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == 1; }));
  });

  qunsetenv("SCORE_EXAMPLES_MANIFEST");
}

TEST_CASE("every section of the online examples is open at first", "[startscreen]")
{
  QTemporaryDir site;
  REQUIRE(site.isValid());
  const auto file = site.filePath("example.score");
  writeFile(file, "{}");
  // Three sections, one of them with two groups and one without any.
  const char* entries[][3] = {
      {"common-practices/3d/a", "common-practices", "3d"},
      {"examples/audio/b", "examples", "audio"},
      {"examples/video/c", "examples", "video"},
      {"examples/video/d", "examples", "video"},
      {"in-depth/e", "in-depth", ""},
      {"reference/processes/f", "reference", "processes"},
  };
  QString list;
  for(auto& [id, section, group] : entries)
  {
    if(!list.isEmpty())
      list += ",";
    list += QStringLiteral(
                R"({"id":"%1","name":"%1","section":"%2","group":"%3","file":"%4",)"
                R"("format":"score","size":2,"platforms":[]})")
                .arg(id, section, group, QUrl::fromLocalFile(file).toString());
  }
  writeFile(
      site.filePath("index.json"),
      QStringLiteral(R"({"version":1,"examples":[%1]})").arg(list).toUtf8());
  qputenv(
      "SCORE_EXAMPLES_MANIFEST",
      QUrl::fromLocalFile(site.filePath("index.json")).toString().toUtf8());
  qputenv("XDG_CACHE_HOME", site.filePath("cache").toUtf8());

  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    UserLibrary lib;
    Screen s;
    showPage(*s.screen, "Online examples");
    REQUIRE(waitFor([&] { return !visibleCards(*s.screen).empty(); }));
    QApplication::processEvents();

    std::set<QString> shown;
    for(auto* card : visibleCards(*s.screen))
      shown.insert(card->toolTip().section(QLatin1Char('\n'), 0, 0));
    for(auto& e : entries)
    {
      INFO(e[0]);
      CHECK(shown.count(e[0]) == 1);
    }
  });

  qunsetenv("SCORE_EXAMPLES_MANIFEST");
}

TEST_CASE("the package manager installs the default library for the start screen", "[startscreen]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    UserLibrary lib;
    // Where the package manager installs packages.
    struct RootPath
    {
      Library::Settings::Model& settings;
      QString previous = settings.getRootPath();
      ~RootPath() { settings.setRootPath(previous); }
    } root{ctx.settings<Library::Settings::Model>()};
    root.settings.setRootPath(lib.root);

    auto& pm = ctx.settings<PM::PluginSettingsModel>();
    // Disconnects what records into the locals below, however the test ends.
    QObject recorder;
    QStringList dialogs;
    QObject::connect(
        &pm, &PM::PluginSettingsModel::information, &recorder,
        [&](const QString& title, const QString&) { dialogs.push_back(title); });
    QObject::connect(
        &pm, &PM::PluginSettingsModel::warning, &recorder,
        [&](const QString& title, const QString&) { dialogs.push_back(title); });

    auto& defaultLibrary = score::DefaultLibrary::instance();
    using State = score::DefaultLibrary::State;
    std::vector<State> states;
    QObject::connect(&defaultLibrary, &score::DefaultLibrary::changed, &recorder, [&] {
      states.push_back(defaultLibrary.state());
    });

    Screen s;
    const auto pressInstall = [&] {
      auto install = visibleItems(*s.screen, [](const QString& t) {
        return t.startsWith("Install the default library") || t == "Try again";
      });
      REQUIRE(install.size() == 1);
      install.front()->labelPressed({});
    };

    SECTION("a failed download is told on the start screen only")
    {
      pm.handleAddon(defaultLibraryEntry(
          QUrl::fromLocalFile(lib.dir.filePath("site/missing.zip")), 2));
      pressInstall();
      REQUIRE(waitFor([&] { return defaultLibrary.state() == State::Failed; }));
      CHECK(hintShown(*s.screen, "The default library could not be installed: "));
      CHECK(visibleItems(*s.screen, "Try again").size() == 1);
      CHECK(dialogs.empty());
    }

    // The package list's archive, which holds the package in one folder.
    const auto zip = lib.dir.filePath("site/default.zip");
    writeZip(
        zip, {{"score-user-library-master/Templates/Installation.score", "{}"},
              {"score-user-library-master/README.md", "The default library"}});
    pm.handleAddon(defaultLibraryEntry(QUrl::fromLocalFile(zip), 2));
    REQUIRE(defaultLibrary.availableVersion() == 2);

    states.clear();
    pressInstall();
    REQUIRE(waitFor([&] { return defaultLibrary.state() == State::Current; }));
    CHECK(std::find(states.begin(), states.end(), State::Installing) != states.end());
    CHECK(score::DefaultLibrary::installedVersion() == 2);
    CHECK(QFile::exists(lib.root + "/packages/default/Templates/Installation.score"));
    // The start screen tells it: no dialog on top of it.
    CHECK(dialogs.empty());
    CHECK(hintShown(*s.screen, "The default library is installed."));

    showPage(*s.screen, "Templates");
    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == 1; }));

    // Installed from the package manager's page: its own dialog tells it.
    const auto& listed = pm.remotePlugins.addons();
    auto pkg = std::find_if(listed.rbegin(), listed.rend(), [](const PM::Package& p) {
      return p.uuid() == score::DefaultLibrary::key();
    });
    REQUIRE(pkg != listed.rend());
    pm.installLibrary(*pkg);
    REQUIRE(waitFor([&] { return dialogs.size() == 1; }));
    CHECK(dialogs.front() == "Package downloaded");
    CHECK(defaultLibrary.state() == State::Current);

    defaultLibrary.setAvailable(-1, 0);
  });
}

TEST_CASE("an online example's menu outlives its card", "[startscreen]")
{
  QTemporaryDir site;
  REQUIRE(site.isValid());
  const auto file = site.filePath("looper.score");
  writeFile(file, "{}");
  writeFile(
      site.filePath("index.json"),
      QStringLiteral(R"({"version":1,"examples":[
{"id":"examples/audio/looper","name":"Looper","section":"examples","group":"audio",
 "file":"%1","format":"score","size":2,"platforms":[]}]})")
          .arg(QUrl::fromLocalFile(file).toString())
          .toUtf8());
  qputenv(
      "SCORE_EXAMPLES_MANIFEST",
      QUrl::fromLocalFile(site.filePath("index.json")).toString().toUtf8());
  qputenv("XDG_CACHE_HOME", site.filePath("cache").toUtf8());

  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    UserLibrary lib;
    const auto copy = lib.root + "/Examples/score-docs/examples/audio/looper.score";
    writeFile(copy, "edited");
    QSettings{}.setValue("score-docs/downloaded/examples/audio/looper", 1);
    writePackageJson(lib.root, 1);
    auto& defaultLibrary = score::DefaultLibrary::instance();

    Screen s;
    showPage(*s.screen, "Online examples");
    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == 1; }));
    REQUIRE(visibleCards(*s.screen).front()->badge() == "Updated online");

    // Opens the menu of the card, then rebuilds the example pages as an install
    // of the default library does, the card that opened the menu with them.
    const auto menuWithoutItsCard = [&] {
      QPointer<score::ExampleCard> card = visibleCards(*s.screen).front();
      // A menu that blocked would wait here for a click: closed after a while,
      // the checks below then fail instead of the test hanging.
      QTimer::singleShot(3000, [] {
        if(auto* popup = QApplication::activePopupWidget())
          popup->close();
      });
      const QPoint pos{20, 20};
      QContextMenuEvent ev{QContextMenuEvent::Mouse, pos, card->mapToGlobal(pos)};
      QApplication::sendEvent(card, &ev);
      QPointer<QMenu> menu = openMenu(*s.screen);
      REQUIRE(menu);

      defaultLibrary.setProgress(0, 0);
      defaultLibrary.setFinished(true);
      REQUIRE(waitFor([&] { return !card; }));
      REQUIRE(menu);
      return menu;
    };

    auto menu = menuWithoutItsCard();

    // The copy is replaced once the user agreed, and the cards rebuilt while
    // the question waits for the answer.
    QString question;
    QTimer answer;
    answer.setInterval(50);
    QObject::connect(&answer, &QTimer::timeout, [&] {
      for(auto* w : QApplication::topLevelWidgets())
      {
        auto box = qobject_cast<QMessageBox*>(w);
        if(!box || !box->isVisible())
          continue;
        question = box->text();
        defaultLibrary.setProgress(0, 0);
        defaultLibrary.setFinished(true);
        box->button(QMessageBox::Yes)->click();
        answer.stop();
      }
    });
    answer.start();

    auto replace = menuAction(*menu, "Download the new online version");
    REQUIRE(replace);
    replace->trigger();
    REQUIRE(waitFor([&] {
      auto cards = visibleCards(*s.screen);
      return cards.size() == 1 && cards.front()->badge() == "In your library";
    }));
    CHECK(question.startsWith("Replace your copy of Looper with the online version?"));
    CHECK(question.contains("looper.score"));
    {
      QFile f{copy};
      REQUIRE(f.open(QIODevice::ReadOnly));
      CHECK(f.readAll() == "{}");
    }

    // The other actions of the same menu still open the example.
    QStringList opened;
    QObject::connect(s.screen, &score::StartScreen::openFile, s.screen, [&](const QString& path) {
      opened.push_back(path);
    });
    menu = menuWithoutItsCard();
    auto open = menuAction(*menu, "Open the example file");
    REQUIRE(open);
    open->trigger();
    REQUIRE(waitFor([&] { return opened.size() == 1; }));
    CHECK(opened.front() == copy);
    menu->close();
  });

  qunsetenv("SCORE_EXAMPLES_MANIFEST");
}


TEST_CASE("the example list's rows are evenly spaced bands and cards", "[startscreen]")
{
  QTemporaryDir site;
  REQUIRE(site.isValid());
  const auto file = site.filePath("example.score");
  writeFile(file, "{}");
  // A section with two groups, one with one, one without any; enough cards in
  // a group for two rows.
  QStringList ids{"a/x/1", "a/x/2", "a/y/3", "b/z/4", "c/5"};
  for(int i = 0; i < 12; i++)
    ids.push_back(QStringLiteral("a/y/many%1").arg(i));
  QString list;
  for(const auto& id : ids)
  {
    const auto parts = id.split('/');
    if(!list.isEmpty())
      list += ",";
    list += QStringLiteral(
                R"({"id":"%1","name":"%1","section":"%2","group":"%3","file":"%4",)"
                R"("format":"score","size":2,"platforms":[]})")
                .arg(id, parts[0], parts.size() == 3 ? parts[1] : QString{},
                     QUrl::fromLocalFile(file).toString());
  }
  writeFile(
      site.filePath("index.json"),
      QStringLiteral(R"({"version":1,"examples":[%1]})").arg(list).toUtf8());
  qputenv(
      "SCORE_EXAMPLES_MANIFEST",
      QUrl::fromLocalFile(site.filePath("index.json")).toString().toUtf8());
  qputenv("XDG_CACHE_HOME", site.filePath("cache").toUtf8());

  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    UserLibrary lib;
    Screen s;
    showPage(*s.screen, "Online examples");
    REQUIRE(waitFor([&] { return visibleCards(*s.screen).size() == std::size_t(ids.size()); }));
    QApplication::processEvents();

    using Drawer = score::CategoryDrawer;
    std::vector<Drawer*> drawers;
    for(auto* w : s.screen->findChildren<QWidget*>())
      if(auto d = dynamic_cast<Drawer*>(w); d && d->isVisibleTo(s.screen))
        drawers.push_back(d);
    REQUIRE(drawers.size() == 3 + 3);
    QWidget* list = drawers.front()->parentWidget();
    while(dynamic_cast<Drawer*>(list))
      list = list->parentWidget();
    auto* viewport = list->parentWidget();
    REQUIRE(viewport);
    CHECK(list->width() == viewport->width());

    struct row
    {
      QRect rect;
      bool band{};
    };
    std::vector<row> rows;
    for(auto* d : drawers)
    {
      const QRect r{d->header()->mapTo(list, QPoint{}), d->header()->size()};
      const bool section = d->parentWidget() == list;
      INFO("band at " << r.x() << "," << r.y() << " " << r.width() << "x" << r.height());
      // Sections start at the list's edge, subsections a step in; every band
      // runs to the scroll bar.
      CHECK(r.left() == (section ? 0 : Drawer::Step));
      CHECK(r.right() == list->width() - 1);
      CHECK(r.height() == Drawer::BandHeight);
      rows.push_back({r, true});
    }
    for(auto* c : visibleCards(*s.screen))
    {
      const QRect r{c->mapTo(list, QPoint{}), c->size()};
      auto it = std::find_if(rows.begin(), rows.end(), [&](const row& x) {
        return !x.band && x.rect.top() == r.top();
      });
      if(it == rows.end())
        rows.push_back({r, false});
      else
        it->rect |= r;
    }
    std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) {
      return a.rect.top() < b.rect.top();
    });

    // The first band is a gap from the top; every row, band or cards, is a gap
    // below the row before it, and cards start at their band's left edge.
    REQUIRE(rows.front().band);
    CHECK(rows.front().rect.top() == Drawer::Gap);
    QRect band;
    for(std::size_t i = 0; i < rows.size(); i++)
    {
      const auto& r = rows[i];
      INFO("row " << i << " at " << r.rect.y() << (r.band ? " band" : " cards"));
      if(r.band)
        band = r.rect;
      else
        CHECK(r.rect.left() == band.left());
      if(i > 0 && (r.band || rows[i - 1].band))
        CHECK(r.rect.top() - rows[i - 1].rect.bottom() - 1 == Drawer::Gap);
    }
  });

  qunsetenv("SCORE_EXAMPLES_MANIFEST");
}
