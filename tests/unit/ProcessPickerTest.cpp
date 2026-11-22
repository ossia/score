// The process picker: what the library lists, found by typing, chosen with
// the keyboard alone, with a preset.

#include <Process/ApplicationPlugin.hpp>
#include <Process/ProcessList.hpp>

#include <Automation/AutomationModel.hpp>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QSettings>
#include <QTreeView>
#include <QTreeWidget>

#include <Library/ProcessEntry.hpp>
#include <Library/ProcessPicker.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <optional>

namespace
{
const auto automation = Metadata<ConcreteKey_k, Automation::ProcessModel>::get();

QString nameOf(const score::GUIApplicationContext& ctx, const UuidKey<Process::ProcessModel>& k)
{
  return ctx.interfaces<Process::ProcessFactoryList>().get(k)->prettyName();
}

void press(QWidget* w, int key, const QString& text = {})
{
  QKeyEvent ev{QEvent::KeyPress, key, Qt::NoModifier, text};
  QApplication::sendEvent(w, &ev);
}

void type(QWidget* w, const QString& text)
{
  for(QChar c : text)
    press(w, c == ' ' ? int(Qt::Key_Space) : int(c.toUpper().unicode()), QString{c});
}

struct Picker
{
  explicit Picker(
      const score::GUIApplicationContext& ctx, const Library::ProcessesItemModel& lib,
      Process::ProcessFlags flags = Process::ProcessFlags::SupportsTemporal,
      Library::PickerOptions options = {})
  {
    picker = new Library::ProcessPicker{
        ctx, lib, flags, [this](const Library::PickedProcess& p) { got = p; }, nullptr,
        std::move(options)};
    search = picker->findChild<QLineEdit*>();
    categories = picker->findChild<QTreeWidget*>();
    // The processes: the tree view that is not the categories' tree widget
    for(auto view : picker->findChildren<QTreeView*>())
      if(!qobject_cast<QTreeWidget*>(view))
        processes = view;
    presets = picker->findChild<QListWidget*>();
  }
  ~Picker() { delete picker.data(); }

  QPointer<Library::ProcessPicker> picker;
  QLineEdit* search{};
  QTreeWidget* categories{};
  QTreeView* processes{};
  QListWidget* presets{};
  std::optional<Library::PickedProcess> got;
};
}

TEST_CASE("Typing finds a process in any category, the best match first", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QSettings{}.remove("Library/RecentProcesses");
    Library::ProcessesItemModel lib{ctx, nullptr};
    const QString name = nameOf(ctx, automation);

    Picker p{ctx, lib};
    p.picker->setSearch(name);
    REQUIRE_FALSE(p.picker->processes().isEmpty());
    CHECK(p.picker->processes().front() == name);

    // Part of a word, any case
    p.picker->setSearch(name.mid(1, 4).toUpper());
    CHECK(p.picker->processes().contains(name));

    p.picker->setSearch("zzqqxxnotaprocess");
    CHECK(p.picker->processes().isEmpty());

    p.picker->setSearch(name);
    REQUIRE(p.picker->accept());
    REQUIRE(p.got);
    CHECK(p.got->key == automation);
    CHECK_FALSE(p.got->preset);
  });
}

TEST_CASE("The picker offers only what can go where it was opened", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    // A process for the timeline that cannot go in a state
    const Process::ProcessModelFactory* temporalOnly{};
    for(auto& f : ctx.interfaces<Process::ProcessFactoryList>())
    {
      const auto fl = f.flags();
      if((fl & Process::ProcessFlags::SupportsTemporal)
         && !(fl & Process::ProcessFlags::SupportsState)
         && !(fl & Process::ProcessFlags::Deprecated))
      {
        temporalOnly = &f;
        break;
      }
    }
    REQUIRE(temporalOnly);

    Picker inTimeline{ctx, lib, Process::ProcessFlags::SupportsTemporal};
    inTimeline.picker->setSearch(temporalOnly->prettyName());
    CHECK(inTimeline.picker->processes().contains(temporalOnly->prettyName()));

    Picker inState{ctx, lib, Process::ProcessFlags::SupportsState};
    inState.picker->setSearch(temporalOnly->prettyName());
    CHECK_FALSE(inState.picker->processes().contains(temporalOnly->prettyName()));
  });
}

TEST_CASE("The keyboard alone picks a process and one of its presets", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QSettings{}.remove("Library/RecentProcesses");
    // Scanning the library starts its presets over: added after
    Library::ProcessesItemModel lib{ctx, nullptr};
    auto& plugin = ctx.applicationPlugin<Process::ApplicationPlugin>();
    {
      Process::Preset preset;
      preset.name = "Zebra sweep";
      preset.key = {automation, {}};
      plugin.addPreset(std::move(preset));
    }
    const QString name = nameOf(ctx, automation);

    SECTION("A preset found by its name")
    {
      Picker p{ctx, lib};
      type(p.search, "zebra sw");
      REQUIRE_FALSE(p.picker->processes().isEmpty());
      CHECK(p.picker->processes().front() == name + " / Zebra sweep");
      CHECK(p.presets->currentItem()->text() == "Zebra sweep");

      press(p.search, Qt::Key_Return);
      REQUIRE(p.got);
      CHECK(p.got->key == automation);
      REQUIRE(p.got->preset);
      CHECK(p.got->preset->name == "Zebra sweep");
    }

    SECTION("A process, then Right to its presets")
    {
      Picker p{ctx, lib};
      type(p.search, name);
      CHECK(p.picker->presets() == (QStringList{"Default", "Zebra sweep"}));
      CHECK(p.presets->currentRow() == 0);

      press(p.search, Qt::Key_Right);
      press(p.presets, Qt::Key_Down);
      press(p.presets, Qt::Key_Return);
      REQUIRE(p.got);
      CHECK(p.got->key == automation);
      REQUIRE(p.got->preset);
      CHECK(p.got->preset->name == "Zebra sweep");
    }

    SECTION("Typing in a list goes on in the search")
    {
      Picker p{ctx, lib};
      type(p.processes, name);
      CHECK(p.search->text() == name);
      CHECK(p.picker->processes().front() == name);
    }

    SECTION("Escape closes it without a choice")
    {
      Picker p{ctx, lib};
      type(p.search, name);
      press(p.search, Qt::Key_Escape);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      CHECK_FALSE(p.got);
      CHECK_FALSE(p.picker);
    }

    for(int i = int(plugin.presets.size()) - 1; i >= 0; i--)
      if(plugin.presets[i].name == "Zebra sweep")
        plugin.removePreset(i);
  });
}

TEST_CASE("The categories list what the library has, and what was picked last", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QSettings{}.remove("Library/RecentProcesses");
    Library::ProcessesItemModel lib{ctx, nullptr};
    const QString name = nameOf(ctx, automation);
    {
      Picker p{ctx, lib};
      CHECK(p.picker->categories().front() == "All");
      CHECK_FALSE(p.picker->categories().contains("Recent"));
      // Snippets are no process
      CHECK_FALSE(p.picker->categories().contains("Snippets"));
      p.picker->setSearch(name);
      REQUIRE(p.picker->accept());
    }
    {
      Picker p{ctx, lib};
      CHECK(p.picker->categories().front() == "Recent");
      // Opens on them, the search empty
      CHECK(p.picker->processes() == QStringList{name});

      // Down in the categories shows another one's processes
      press(p.categories, Qt::Key_Down);
      CHECK(p.picker->processes().size() > 1);
    }
  });
}

TEST_CASE("From a port, only the processes that can be cabled to it", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    // A process that says it has no audio inlet, and one that has one
    const Process::ProcessModelFactory* without{};
    const Process::ProcessModelFactory* with{};
    for(auto& f : ctx.interfaces<Process::ProcessFactoryList>())
    {
      const auto fl = f.flags();
      if(!(fl & Process::ProcessFlags::SupportsTemporal)
         || (fl & Process::ProcessFlags::Deprecated))
        continue;
      const auto d = f.descriptor({});
      if(!d.inlets)
        continue;
      const bool audio = std::find(d.inlets->begin(), d.inlets->end(), Process::PortType::Audio)
                         != d.inlets->end();
      if(audio && !with)
        with = &f;
      if(!audio && !without)
        without = &f;
    }
    REQUIRE(without);

    Library::ProcessPicker picker{
        ctx, lib, Process::ProcessFlags::SupportsTemporal, {}, nullptr,
        Library::PickerOptions{Library::PortFilter{Process::PortType::Audio, true}}};
    picker.setSearch(without->prettyName());
    CHECK_FALSE(picker.processes().contains(without->prettyName()));
    if(with)
    {
      picker.setSearch(with->prettyName());
      CHECK(picker.processes().contains(with->prettyName()));
    }

    // The tag saying so takes the filter away: everything is offered again
    auto clear = picker.findChild<QToolButton*>("ProcessPickerChipClear");
    REQUIRE(clear);
    clear->click();
    CHECK(clear->parentWidget()->isHidden());
    picker.setSearch(without->prettyName());
    CHECK(picker.processes().contains(without->prettyName()));
  });
}

namespace
{
//! A preset of the automation, as the library has them
void addPreset(Process::ApplicationPlugin& plugin, const QString& name)
{
  Process::Preset preset;
  preset.name = name;
  preset.key = {automation, {}};
  preset.path = "/presets/" + name + ".scp";
  plugin.addPreset(std::move(preset));
}

//! Lets queued work (zero timers) run
void settle()
{
  QEventLoop loop;
  QTimer::singleShot(20, &loop, &QEventLoop::quit);
  loop.exec();
}

void removePresets(Process::ApplicationPlugin& plugin)
{
  for(int i = int(plugin.presets.size()) - 1; i >= 0; i--)
    if(plugin.presets[i].key.key == automation)
      plugin.removePreset(i);
}
}

TEST_CASE(
    "Presets coming while the picker is open keep the one chosen, the list shown "
    "again once",
    "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    auto& plugin = ctx.applicationPlugin<Process::ApplicationPlugin>();
    addPreset(plugin, "Middle");
    addPreset(plugin, "Zebra");

    Picker p{ctx, lib};
    p.picker->setSearch(nameOf(ctx, automation));
    const auto shown = p.picker->presets();
    REQUIRE(shown.size() == 3);
    REQUIRE(shown.front() == "Default");
    p.presets->setCurrentRow(shown.indexOf("Zebra"));

    // Many come, some of them before it in the list: indices move
    for(const char* name : {"Alpha", "Beta", "Gamma"})
      addPreset(plugin, name);
    int searches = 0;
    QObject::connect(p.processes->model(), &QAbstractItemModel::modelReset, [&] { searches++; });
    settle();
    CHECK(p.picker->presets().size() == 6);
    CHECK(searches == 1);
    CHECK(p.presets->currentItem()->text() == "Zebra");

    REQUIRE(p.picker->accept());
    REQUIRE(p.got);
    REQUIRE(p.got->preset);
    CHECK(p.got->preset->name == "Zebra");
    removePresets(plugin);
  });
}

TEST_CASE("Presets cleared while the picker is open are no longer offered", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    auto& plugin = ctx.applicationPlugin<Process::ApplicationPlugin>();
    addPreset(plugin, "Zebra");

    Picker p{ctx, lib};
    p.picker->setSearch(nameOf(ctx, automation));
    p.presets->setCurrentRow(1);
    plugin.clearPresets();
    settle();
    CHECK(p.picker->presets() == QStringList{"Default"});
    REQUIRE(p.picker->accept());
    REQUIRE(p.got);
    CHECK_FALSE(p.got->preset);
  });
}

TEST_CASE("Where no preset can be loaded, the picker offers none", "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    auto& plugin = ctx.applicationPlugin<Process::ApplicationPlugin>();
    addPreset(plugin, "Zebra");

    Picker p{ctx, lib, Process::ProcessFlags::SupportsTemporal, {{}, false}};
    CHECK(p.presets->isHidden());
    p.picker->setSearch("Zebra");
    for(const auto& name : p.picker->processes())
      CHECK_FALSE(name.contains("Zebra"));
    p.picker->setSearch(nameOf(ctx, automation));
    REQUIRE(p.picker->accept());
    REQUIRE(p.got);
    CHECK_FALSE(p.got->preset);
    removePresets(plugin);
  });
}

TEST_CASE(
    "The picker keeps its own keys from the application's shortcuts, not the "
    "others",
    "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    Picker p{ctx, lib};
    const auto overridden = [&](int key, Qt::KeyboardModifiers mods) {
      QKeyEvent ev{QEvent::ShortcutOverride, key, mods};
      ev.ignore();
      QApplication::sendEvent(p.search, &ev);
      return ev.isAccepted();
    };
    CHECK(overridden(Qt::Key_Space, Qt::NoModifier));
    CHECK(overridden(Qt::Key_Escape, Qt::NoModifier));
    CHECK(overridden(Qt::Key_Up, Qt::NoModifier));
    // Save, Quit: still the application's. Undo is the field's own.
    CHECK_FALSE(overridden(Qt::Key_S, Qt::ControlModifier));
    CHECK_FALSE(overridden(Qt::Key_Q, Qt::ControlModifier));
  });
}

TEST_CASE(
    "A picker lists the library as it is when opened, while one open keeps its own",
    "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    lib.flushPending();
    Picker before{ctx, lib};
    before.picker->setSearch("Kick from the scan");
    CHECK(before.picker->processes().isEmpty());

    // A file of a process found by a scan, as the library publishes them
    Library::ProcessEntry e;
    e.rootKey = automation;
    e.node = Library::StagedNode{
        {{automation, "Kick from the scan", "/a/file"}, {}}, {}};
    lib.publish(std::move(e));
    lib.flushPending();

    Picker after{ctx, lib};
    after.picker->setSearch("Kick from the scan");
    CHECK(after.picker->processes() == QStringList{"Kick from the scan"});
    before.picker->setSearch("Kick from");
    CHECK(before.picker->processes().isEmpty());
  });
}

TEST_CASE(
    "A category lists its own processes first, then those of its folders under "
    "their names, as the library panel has them",
    "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QSettings{}.remove("Library/RecentProcesses");
    Library::ProcessesItemModel lib{ctx, nullptr};
    // PickerTest: a process, a process with an entry of its own, a folder
    auto& root = lib.rootNode();
    const auto node = [](const QString& name, const QString& data, bool process) {
      Library::ProcessData d;
      if(process)
        d.key = automation;
      d.prettyName = name;
      d.customData = data;
      return d;
    };
    auto& cat = root.emplace_back(node("PickerTest", {}, false), &root);
    cat.emplace_back(node("Alone", "alone", true), &cat);
    auto& host = cat.emplace_back(node("Host", "host", true), &cat);
    host.emplace_back(node("Own entry", "own", true), &host);
    auto& sub = cat.emplace_back(node("Sub", {}, false), &cat);
    sub.emplace_back(node("Deeper", "deeper", true), &sub);

    Picker p{ctx, lib};
    QTreeWidgetItem* item{};
    for(int i = 0; i < p.categories->topLevelItemCount(); i++)
      if(p.categories->topLevelItem(i)->text(0) == "PickerTest")
        item = p.categories->topLevelItem(i);
    REQUIRE(item);

    // The process with entries is a folder too, as in the panel
    QStringList folders;
    for(int i = 0; i < item->childCount(); i++)
      folders.push_back(item->child(i)->text(0));
    CHECK(folders == (QStringList{"Host", "Sub"}));

    p.categories->setCurrentItem(item);
    QStringList shown;
    auto* model = p.processes->model();
    for(int r = 0; r < model->rowCount(); r++)
      shown.push_back(model->index(r, 0).data().toString());
    CHECK(shown == (QStringList{"Alone", "Host", "Host", "Own entry", "Sub", "Deeper"}));
    CHECK(p.picker->processes() == (QStringList{"Alone", "Host", "Own entry", "Deeper"}));

    // The headings are passed over by the keys, and cannot be chosen
    CHECK(p.processes->currentIndex().row() == 0);
    CHECK_FALSE(model->flags(model->index(1, 0)) & Qt::ItemIsSelectable);
    p.processes->setFocus();
    press(p.processes, Qt::Key_Down);
    CHECK(p.processes->currentIndex().row() == 2);
    REQUIRE(p.picker->accept());
    REQUIRE(p.got);
    CHECK(p.got->customData == "host");

    // The process's folder: the process, then its entries
    Picker q{ctx, lib};
    for(int i = 0; i < q.categories->topLevelItemCount(); i++)
      if(q.categories->topLevelItem(i)->text(0) == "PickerTest")
        q.categories->setCurrentItem(q.categories->topLevelItem(i)->child(0));
    CHECK(q.picker->processes() == (QStringList{"Host", "Own entry"}));
  });
}

TEST_CASE(
    "The picker is moved by dragging it, and found processes say where they are",
    "[library][picker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel lib{ctx, nullptr};
    Picker p{ctx, lib};

    // Entries of the same name differ by where they are: all of it is shown
    const QString name = nameOf(ctx, automation);
    p.picker->setSearch(name);
    REQUIRE_FALSE(p.picker->processes().isEmpty());
    const auto first = p.processes->model()->index(0, 0);
    const QString where = first.data(Qt::UserRole + 10).toString();
    CHECK_FALSE(where.isEmpty());
    CHECK(first.data(Qt::ToolTipRole).toString() == where + " / " + name);

    // A pop-up is no window of the window manager: it moves itself
    p.picker->move(100, 100);
    const QPointF grab{5, 5};
    const auto send = [&](QEvent::Type t, QPointF global, Qt::MouseButtons buttons) {
      QMouseEvent ev{t, grab, global, Qt::LeftButton, buttons, Qt::NoModifier};
      QApplication::sendEvent(p.picker, &ev);
    };
    send(QEvent::MouseButtonPress, QPointF{105, 105}, Qt::LeftButton);
    send(QEvent::MouseMove, QPointF{145, 125}, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, QPointF{145, 125}, Qt::NoButton);
    CHECK(p.picker->pos() == QPoint{140, 120});
    // Moving afterwards does nothing
    send(QEvent::MouseMove, QPointF{300, 300}, Qt::NoButton);
    CHECK(p.picker->pos() == QPoint{140, 120});
  });
}
