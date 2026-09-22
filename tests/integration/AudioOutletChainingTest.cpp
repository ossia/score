// Routing follows the end of an audio chain. Appending a process moves the
// explicit address and the propagation flag forward onto the new tail;
// shift-deleting the tail moves them back onto whatever fed it. The gain trim
// never moves: chained outlets are in series. The cable commands are covered
// too: they must restore the propagation they saved, never assume it was on.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGuiApplication>

#include <qpa/qwindowsysteminterface.h>

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

namespace
{
// Media::Merger: one audio outlet, eight audio inlets. Enough to chain two of
// them and to type-match every port this test touches. By key rather than by
// type, as the media plug-in does not export its process classes.
const UuidKey<Process::ProcessModel> mergerKey
    = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7"));

const State::AddressAccessor testAddress{State::Address{"test", {"out"}}};

Process::ProcessModel& addMerger(score::Document& doc, Scenario::IntervalModel& itv)
{
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto* proc = m.createProcessInNewSlot(itv, mergerKey, {});
  m.commit();
  REQUIRE(proc != nullptr);
  return *proc;
}

std::vector<Process::ProcessModel*> mergers(Scenario::IntervalModel& itv)
{
  std::vector<Process::ProcessModel*> v;
  for(auto& p : itv.processes)
    if(p.concreteKey() == mergerKey)
      v.push_back(&p);
  return v;
}

Process::ProcessModel* newMerger(
    Scenario::IntervalModel& itv, const std::vector<Process::ProcessModel*>& before)
{
  for(auto* p : mergers(itv))
    if(std::find(before.begin(), before.end(), p) == before.end())
      return p;
  return nullptr;
}

Process::AudioOutlet& audioOut(Process::ProcessModel& p)
{
  auto* out = qobject_cast<Process::AudioOutlet*>(p.outlets().front());
  REQUIRE(out != nullptr);
  return *out;
}

// ScenarioEditor::remove reads qApp->keyboardModifiers(), which only the
// platform layer writes: a synthesized QKeyEvent would not reach it.
void holdShift(bool held)
{
  const auto mods = held ? Qt::ShiftModifier : Qt::NoModifier;
  QWindowSystemInterface::handleKeyEvent(
      nullptr, held ? QEvent::KeyPress : QEvent::KeyRelease, Qt::Key_Shift, mods);
  QWindowSystemInterface::flushWindowSystemEvents();
  REQUIRE(QGuiApplication::keyboardModifiers() == mods);
}

bool removeSelected(const score::DocumentContext& ctx, Process::ProcessModel& proc)
{
  const Selection sel{&proc};
  for(auto& iface : ctx.app.interfaces<score::ObjectEditorList>())
    if(iface.remove(sel, ctx))
      return true;
  return false;
}
}

TEST_CASE(
    "An audio chain carries its routing at the end",
    "[integration][command][undo][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& itv = dm.baseInterval();

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    auto& stack = doc->commandStack();
    const Process::ProcessData data{mergerKey, QStringLiteral("Merger"), {}};

    // ---- Appending moves the routing forward.
    auto& head = addMerger(*doc, itv);
    auto& out = audioOut(head);

    const double defaultGain = out.gain();
    out.setAddress(testAddress);
    out.setPropagate(false);
    out.setGain(0.25);

    auto before = mergers(itv);
    Scenario::createProcessAfterPort(*presenter, data, {}, {}, head, out, true);

    auto* tail = newMerger(itv, before);
    REQUIRE(tail != nullptr);
    auto& newOut = audioOut(*tail);

    CHECK(out.cables().size() == 1);
    CHECK(newOut.address() == testAddress);
    CHECK(newOut.propagate() == false);
    CHECK(out.address() == State::AddressAccessor{});
    CHECK(out.propagate() == false);

    // The two outlets are in series: moving the trim would change the level.
    CHECK(out.gain() == 0.25);
    CHECK(newOut.gain() == defaultGain);

    // One macro, one undo step, the exact state back.
    REQUIRE(stack.canUndo());
    stack.undo();

    CHECK(newMerger(itv, before) == nullptr);
    CHECK(out.cables().empty());
    CHECK(out.address() == testAddress);
    CHECK(out.propagate() == false);
    CHECK(out.gain() == 0.25);

    // ---- Cabling and uncabling an outlet whose propagation the user turned
    // off must leave it off.
    auto& sink = addMerger(*doc, itv);
    auto& sinkIn = *sink.inlets().front();

    CommandDispatcher<> disp{doc->context().commandStack};

    const auto c1 = getStrongId(dm.cables);
    disp.submit<Dataflow::CreateCable>(
        dm, c1, Process::CableType::ImmediateGlutton, out, sinkIn);
    CHECK(out.propagate() == false);

    disp.submit<Dataflow::RemoveCable>(dm, dm.cables.at(c1));
    CHECK(out.cables().empty());
    CHECK(out.propagate() == false);

    stack.undo();
    CHECK(out.cables().size() == 1);
    CHECK(out.propagate() == false);
    stack.redo();

    // An outlet that was propagating still stops when it gets a cable, and
    // starts again when that is undone.
    out.setPropagate(true);
    const auto c2 = getStrongId(dm.cables);
    disp.submit<Dataflow::CreateCable>(
        dm, c2, Process::CableType::ImmediateGlutton, out, sinkIn);
    CHECK(out.propagate() == false);

    stack.undo();
    CHECK(out.propagate() == true);

    // ---- Shift-deleting the tail moves the routing back, whatever its value.
    for(const bool propagating : {false, true})
    {
      auto& a = addMerger(*doc, itv);
      auto& aOut = audioOut(a);
      aOut.setAddress(testAddress);
      aOut.setPropagate(propagating);

      before = mergers(itv);
      Scenario::createProcessAfterPort(*presenter, data, {}, {}, a, aOut, true);

      auto* b = newMerger(itv, before);
      REQUIRE(b != nullptr);
      REQUIRE(audioOut(*b).propagate() == propagating);
      REQUIRE(aOut.address() == State::AddressAccessor{});

      holdShift(true);
      REQUIRE(removeSelected(doc->context(), *b));
      holdShift(false);

      CHECK(newMerger(itv, before) == nullptr);
      CHECK(aOut.address() == testAddress);
      CHECK(aOut.propagate() == propagating);

      stack.undo();
      auto* restored = newMerger(itv, before);
      REQUIRE(restored != nullptr);
      CHECK(audioOut(*restored).address() == testAddress);
      CHECK(audioOut(*restored).propagate() == propagating);
      CHECK(aOut.address() == State::AddressAccessor{});
    }

    // The tail's value wins even when the source was left propagating: the
    // state moves, it is not merged.
    {
      auto& a = addMerger(*doc, itv);
      auto& aOut = audioOut(a);
      aOut.setPropagate(true);

      before = mergers(itv);
      Scenario::createProcessAfterPort(*presenter, data, {}, {}, a, aOut, true);

      auto* b = newMerger(itv, before);
      REQUIRE(b != nullptr);
      audioOut(*b).setPropagate(false);
      aOut.setPropagate(true);

      holdShift(true);
      REQUIRE(removeSelected(doc->context(), *b));
      holdShift(false);

      CHECK(aOut.propagate() == false);

      stack.undo();
      CHECK(aOut.propagate() == true);
    }

    // The inspector's port widgets must go away while the document's device
    // list is still alive: a port address combo outlives it otherwise and
    // reloads itself against freed devices on the way out.
    doc->context().selectionStack.deselect();
    QApplication::processEvents();
    QApplication::processEvents();
  });
}
