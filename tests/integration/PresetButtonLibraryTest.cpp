// A process's preset button opens the library at that process: its presets,
// which then load into it, one undo step each, without creating a process.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/PresetTree.hpp>
#include <Process/Process.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
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
