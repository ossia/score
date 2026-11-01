// A process's preset button opens the library at that process: its presets,
// which then load into it, one undo step each, without creating a process.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/PresetTree.hpp>
#include <Process/Process.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/PresetAudition.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/PresetTreeView.hpp>
#include <Library/ProcessTreeView.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/widgets/QGraphicsSelectablePixmapToggle.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <Effect/EffectLayer.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>

#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
// Pattern sequencer: fixed controls
const QString pattern_uuid = QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e");

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
}

QString cursor(const Process::ProcessModel& proc)
{
  const auto c = proc.property("score_presetCursor").toStringList();
  return c.size() == 2 ? c[1] : QString{};
}
}

TEST_CASE(
    "the preset button opens the library at its process, whose presets load into it",
    "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto panel = ctx.findPanel<Library::ProcessPanel>();
    REQUIRE(panel);
    auto& widget = panel->processWidget();
    auto& model = widget.presetModel();
    auto& view = widget.presetView();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, pattern_uuid, {});
    REQUIRE(proc);
    QPointer<Process::ProcessModel> guard = proc;
    auto& interval = score::test::base_interval(*doc);
    const auto processes = interval.processes.size();

    // Presets of the state it is in, in two folders
    auto& plug = ctx.applicationPlugin<Process::ApplicationPlugin>();
    plug.clearPresets();
    for(auto [name, category] : {std::pair{"one", "Basics"}, std::pair{"two", "Basics"},
                                 std::pair{"three", "More/Deep"}})
    {
      auto p = proc->savePreset();
      p.name = name;
      p.category = category;
      plug.addPreset(std::move(p));
    }

    auto btn = std::unique_ptr<score::QGraphicsDraggablePixmap>(
        Process::makePresetButton(*proc, doc->context(), nullptr, nullptr));
    REQUIRE(btn);
    btn->click(Qt::LeftButton, {});
    spin(50);

    // The library is at the process, and its presets are this process's
    CHECK(model.target() == proc);
    CHECK(widget.processView().currentIndex().data().toString() == "Pattern sequencer");
    REQUIRE(model.rowCount({}) == 2);
    CHECK(model.index(0, 0, {}).data().toString() == "Basics");
    CHECK(model.index(1, 0, {}).data().toString() == "More");

    // Double-click (or Enter) loads into it: one undo step, no new process
    const auto steps = doc->commandStack().size();
    const auto basics = model.indexOfFolder({"Basics"});
    const auto two = model.index(1, 0, basics);
    REQUIRE(model.preset(two));
    view.setCurrentIndex(two);
    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(&view, &enter);
    spin(20);
    CHECK(doc->commandStack().size() == steps + 1);
    CHECK(interval.processes.size() == processes);
    CHECK(cursor(*proc) == "two");
    // Shown as its current preset
    CHECK(model.data(two, Qt::FontRole).value<QFont>().bold());
    CHECK(!model.data(model.index(0, 0, basics), Qt::FontRole).value<QFont>().bold());

    // Forward from the button: the library follows
    btn->click(Qt::ForwardButton, {});
    spin(20);
    CHECK(cursor(*proc) == "three");
    REQUIRE(model.preset(view.currentIndex()));
    CHECK(model.preset(view.currentIndex())->name == "three");
    CHECK(view.isExpanded(model.indexOfFolder({"More", "Deep"})));

    // Gone with the process
    while(doc->commandStack().canUndo())
      doc->commandStack().undo();
    spin(20);
    REQUIRE(!guard);
    CHECK(model.target() == nullptr);

    plug.clearPresets();
  });
}

TEST_CASE(
    "choosing another process in the library stops loading into the first",
    "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto panel = ctx.findPanel<Library::ProcessPanel>();
    REQUIRE(panel);
    auto& widget = panel->processWidget();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, pattern_uuid, {});
    REQUIRE(proc);

    auto btn = std::unique_ptr<score::QGraphicsDraggablePixmap>(
        Process::makePresetButton(*proc, doc->context(), nullptr, nullptr));
    btn->click(Qt::LeftButton, {});
    spin(20);
    REQUIRE(widget.presetModel().target() == proc);

    // Its own entry again: still its presets
    widget.processView().setCurrentIndex(widget.processView().currentIndex());
    CHECK(widget.presetModel().target() == proc);

    // Another: the library is browsed for that one
    auto& tree = widget.processView();
    const auto first = tree.model()->index(0, 0, {});
    tree.setCurrentIndex(first);
    spin(20);
    CHECK(widget.presetModel().target() == nullptr);
  });
}

namespace
{
// LFO: fixed controls, among them "Ampl."
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

Process::ControlInlet* control(Process::ProcessModel& proc, const QString& name)
{
  for(auto* in : proc.inlets())
    if(auto c = qobject_cast<Process::ControlInlet*>(in); c && c->name() == name)
      return c;
  return nullptr;
}

float value(Process::ControlInlet& c)
{
  return ossia::convert<float>(c.value());
}

//! An LFO whose library presets set Ampl. to 0.1, 0.2, 0.3, opened from its
//! preset button
struct Audition
{
  const score::GUIApplicationContext& ctx;
  Library::ProcessWidget& widget;
  score::Document* doc{};
  Process::ProcessModel* proc{};
  Process::ControlInlet* ampl{};
  std::unique_ptr<score::QGraphicsDraggablePixmap> btn;

  explicit Audition(const score::GUIApplicationContext& c)
      : ctx{c}
      , widget{c.findPanel<Library::ProcessPanel>()->processWidget()}
  {
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    proc = score::test::add_process(*doc, lfo_uuid, {});
    REQUIRE(proc);
    ampl = control(*proc, "Ampl.");
    REQUIRE(ampl);

    auto& plug = ctx.applicationPlugin<Process::ApplicationPlugin>();
    plug.clearPresets();
    for(auto [name, v] : {std::pair{"a", 0.1f}, std::pair{"b", 0.2f}, std::pair{"c", 0.3f}})
    {
      ampl->setValue(v);
      auto p = proc->savePreset();
      p.name = name;
      plug.addPreset(std::move(p));
    }
    ampl->setValue(0.5f);

    btn.reset(Process::makePresetButton(*proc, doc->context(), nullptr, nullptr));
    btn->click(Qt::LeftButton, {});
    spin(20);
    REQUIRE(widget.presetModel().target() == proc);
  }
  ~Audition() { ctx.applicationPlugin<Process::ApplicationPlugin>().clearPresets(); }

  //! What clicking or moving with the arrows onto a preset does
  void select(const QString& name)
  {
    auto& m = widget.presetModel();
    for(int i = 0; i < m.rowCount({}); i++)
      if(auto idx = m.index(i, 0, {}); m.preset(idx) && m.preset(idx)->name == name)
        widget.presetView().setCurrentIndex(idx);
    spin(5);
  }
  void key(Qt::Key k)
  {
    QKeyEvent ev{QEvent::KeyPress, k, Qt::NoModifier};
    QApplication::sendEvent(&widget.presetView(), &ev);
    spin(5);
  }
  int steps() const { return doc->commandStack().size(); }
};
}

TEST_CASE(
    "moving through the presets plays them, kept as one undo step",
    "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Audition a{ctx};
    const int steps = a.steps();

    // The presets getting the focus as they open: nothing is loaded
    {
      QFocusEvent focus{QEvent::FocusIn, Qt::OtherFocusReason};
      QApplication::sendEvent(&a.widget.presetView(), &focus);
      spin(5);
      CHECK(value(*a.ampl) == 0.5f);
      CHECK(cursor(*a.proc).isEmpty());
    }

    a.select("a");
    CHECK(value(*a.ampl) == 0.1f);
    a.select("c");
    CHECK(value(*a.ampl) == 0.3f);
    a.select("b");
    CHECK(value(*a.ampl) == 0.2f);
    // Heard, not yet in the history
    CHECK(a.steps() == steps);
    CHECK(cursor(*a.proc) == "b");

    a.key(Qt::Key_Return);
    CHECK(a.steps() == steps + 1);
    CHECK(value(*a.ampl) == 0.2f);

    // One undo: back to before the first
    a.doc->commandStack().undo();
    CHECK(value(*a.ampl) == 0.5f);
  });
}

TEST_CASE("Escape goes back to before the presets auditioned", "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Audition a{ctx};
    const int steps = a.steps();
    Process::setPresetCursor(*a.proc, {"", "c"});

    a.select("a");
    a.select("b");
    a.key(Qt::Key_Escape);
    CHECK(value(*a.ampl) == 0.5f);
    CHECK(a.steps() == steps);
    CHECK(cursor(*a.proc) == "c");
  });
}

TEST_CASE("doing something else keeps the preset auditioned", "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Audition a{ctx};
    const int steps = a.steps();

    // The focus leaving the presets, as a click elsewhere does first
    a.select("a");
    QLineEdit elsewhere;
    Q_EMIT qApp->focusChanged(&a.widget.presetView(), &elsewhere);
    CHECK(a.steps() == steps + 1);
    CHECK(value(*a.ampl) == 0.1f);

    // Another command reaching the history: the session goes in before it
    // can be undone
    a.select("c");
    auto* other = score::test::add_process(*a.doc, lfo_uuid, {});
    REQUIRE(other);
    CHECK(a.steps() == steps + 3);
    CHECK(value(*a.ampl) == 0.3f);
    a.doc->commandStack().undo();
    a.doc->commandStack().undo();
    CHECK(value(*a.ampl) == 0.1f);
    a.doc->commandStack().undo();
    CHECK(value(*a.ampl) == 0.5f);
  });
}

TEST_CASE("a process deleted while auditioned ends it", "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Audition a{ctx};
    QPointer<Process::ProcessModel> guard = a.proc;
    a.select("a");
    // The process goes: undoing its creation
    a.doc->commandStack().undo();
    spin(10);
    REQUIRE(!guard);
    CHECK(a.widget.presetModel().target() == nullptr);
    // Nothing of it left to keep
    a.key(Qt::Key_Return);
    a.doc->commandStack().redo();
    CHECK(a.doc->commandStack().size() == 1);
  });
}

TEST_CASE("a preset of another script is not auditioned", "[integration][presets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Audition a{ctx};
    auto p = a.proc->savePreset();
    CHECK(Library::PresetAudition::canAudition(*a.proc, p));
    p.key.effect = "another script";
    CHECK(!Library::PresetAudition::canAudition(*a.proc, p));
  });
}
