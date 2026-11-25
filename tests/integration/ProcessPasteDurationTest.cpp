// A process copied from one box and pasted into another keeps its content
// where it was in time: a piano roll pasted into a shorter box is not
// squeezed into it.
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/MidiProcess.hpp>

#include <score/model/ObjectEditor.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QMimeData>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
void spin_events()
{
  for(int i = 0; i < 20; i++)
    QApplication::processEvents();
}
std::vector<double> noteDates(const Midi::ProcessModel& m)
{
  std::vector<double> r;
  for(auto& n : m.notes)
    r.push_back(n.start() * double(m.duration().impl) / 705600000.);
  std::sort(r.begin(), r.end());
  return r;
}
}

TEST_CASE("A piano roll pasted into a shorter box keeps its notes' dates", "[paste][midi][duration]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& root = score::test::base_interval(*doc);
    auto& scenar = score::test::base_scenario(*doc);

    // Four bars at 120: 8 s, a note on every beat
    constexpr int64_t second = 705600000;
    Midi::ProcessModel* roll{};
    Scenario::IntervalModel* child{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, dctx};
      roll = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      REQUIRE(roll);
      m.submit(new Midi::ReplaceNotes{*roll, {}, 0, 127, TimeVal{8 * second}});
      std::vector<Midi::NoteData> notes;
      for(int i = 0; i < 16; i++)
        notes.emplace_back(i / 16., 0.25 / 16., 60, 100);
      m.submit(new Midi::ReplaceNotes{*roll, notes, 0, 127, TimeVal{8 * second}});
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.5);
      // Three bars and a bit
      child = &m.createIntervalAfter(scenar, from.id(), {TimeVal{6500 * second / 1000}, 0.5});
      m.commit();
    }
    REQUIRE(roll->duration() == TimeVal{8 * second});
    const auto before = noteDates(*roll);
    REQUIRE(before.size() == 16);

    // Copied and pasted as Ctrl+C, Ctrl+V do, the box selected
    // The piano roll selected, then Ctrl+C: what copying processes writes
    score::SelectionDispatcher{dctx.selectionStack}.select(*roll);
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, dctx));
    QMimeData mime;
    mime.setData("text/plain", r.toByteArray());
    score::SelectionDispatcher{dctx.selectionStack}.select(*child);
    spin_events();
    const auto processes_before = child->processes.size();
    bool pasted = false;
    for(auto& e : ctx.interfaces<score::ObjectEditorList>())
      if(!pasted && e.paste(QPoint{}, nullptr, mime, dctx))
        pasted = true;
    REQUIRE(pasted);
    REQUIRE(child->processes.size() == processes_before + 1);
    Midi::ProcessModel* copy{};
    for(auto& p : child->processes)
      if(auto* m = qobject_cast<Midi::ProcessModel*>(&p))
        copy = m;
    REQUIRE(copy);

    // As long as it was: four bars, not the box's 6.5 s
    CHECK(copy->duration() == TimeVal{8 * second});
    const auto after = noteDates(*copy);
    REQUIRE(after.size() == before.size());
    for(std::size_t i = 0; i < after.size(); i++)
      CHECK(std::abs(after[i] - before[i]) < 1e-6);

    // Into a longer box: it grows to it, its notes where they were
    Scenario::IntervalModel* longer{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, dctx};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.8);
      longer = &m.createIntervalAfter(scenar, from.id(), {TimeVal{12 * second}, 0.8});
      m.commit();
    }
    score::SelectionDispatcher{dctx.selectionStack}.select(*longer);
    pasted = false;
    for(auto& e : ctx.interfaces<score::ObjectEditorList>())
      if(!pasted && e.paste(QPoint{}, nullptr, mime, dctx))
        pasted = true;
    REQUIRE(pasted);
    Midi::ProcessModel* grown{};
    for(auto& p : longer->processes)
      if(auto* m = qobject_cast<Midi::ProcessModel*>(&p))
        grown = m;
    REQUIRE(grown);
    CHECK(grown->duration() == TimeVal{12 * second});
    const auto grown_dates = noteDates(*grown);
    REQUIRE(grown_dates.size() == before.size());
    for(std::size_t i = 0; i < grown_dates.size(); i++)
      CHECK(std::abs(grown_dates[i] - before[i]) < 1e-6);
  });
}

TEST_CASE("In Scale mode, a piano roll pasted into a shorter box is scaled to it", "[paste][midi][duration][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& root = score::test::base_interval(*doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& edition = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>().editionSettings();
    struct Restore
    {
      Scenario::EditionSettings& e;
      ExpandMode mode{e.expandMode()};
      ~Restore() { e.setExpandMode(mode); }
    } restore{edition};

    constexpr int64_t second = 705600000;
    Midi::ProcessModel* roll{};
    Scenario::IntervalModel* child{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, dctx};
      roll = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      REQUIRE(roll);
      std::vector<Midi::NoteData> notes;
      for(int i = 0; i < 16; i++)
        notes.emplace_back(i / 16., 0.25 / 16., 60, 100);
      m.submit(new Midi::ReplaceNotes{*roll, notes, 0, 127, TimeVal{8 * second}});
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.5);
      child = &m.createIntervalAfter(scenar, from.id(), {TimeVal{4 * second}, 0.5});
      m.commit();
    }
    REQUIRE(roll->duration() == TimeVal{8 * second});
    const auto before = noteDates(*roll);

    score::SelectionDispatcher{dctx.selectionStack}.select(*roll);
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, dctx));
    QMimeData mime;
    mime.setData("text/plain", r.toByteArray());

    const auto paste = [&] {
      score::SelectionDispatcher{dctx.selectionStack}.select(*child);
      spin_events();
      bool pasted = false;
      for(auto& e : ctx.interfaces<score::ObjectEditorList>())
        if(!pasted && e.paste(QPoint{}, nullptr, mime, dctx))
          pasted = true;
      REQUIRE(pasted);
      Midi::ProcessModel* copy{};
      for(auto& p : child->processes)
        if(auto* m = qobject_cast<Midi::ProcessModel*>(&p))
          copy = m;
      REQUIRE(copy);
      return copy;
    };

    // Scale: 8 s of notes in the 4 s box, at half their dates
    edition.setExpandMode(ExpandMode::Scale);
    auto* scaled = paste();
    CHECK(scaled->duration() == TimeVal{4 * second});
    const auto s = noteDates(*scaled);
    REQUIRE(s.size() == before.size());
    for(std::size_t i = 0; i < s.size(); i++)
      CHECK(std::abs(s[i] - before[i] / 2.) < 1e-6);
    doc->commandStack().undo();
    REQUIRE(child->processes.empty());

    // Not Scale: as long as it was, its notes where they were
    edition.setExpandMode(ExpandMode::GrowShrink);
    auto* kept = paste();
    CHECK(kept->duration() == TimeVal{8 * second});
    const auto k = noteDates(*kept);
    for(std::size_t i = 0; i < k.size(); i++)
      CHECK(std::abs(k[i] - before[i]) < 1e-6);
  });
}
