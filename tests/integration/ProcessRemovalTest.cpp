// Integration test: removing processes that sit in chains, through the Delete
// and Shift+Delete actions.
//
// Delete relinks what the removed processes linked, a run of removed
// processes going as one sub-chain, without duplicating a cable that exists.
// Shift+Delete takes their cables along. Either way, the propagation of an
// audio tail goes back to the outlets that fed it and are left without a
// cable; the address of a tail only goes back with Delete. Each removal is one
// undo step and undoing it brings back the exact cables.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>

#include <algorithm>
#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

namespace
{
// avnd's Float: one value inlet, one value outlet.
const auto floatKey
    = UuidKey<Process::ProcessModel>{"ee3a50c0-a202-4f51-a26d-be57a939997d"};
// Media::Merger: eight audio inlets, one audio outlet.
const auto mergerKey
    = UuidKey<Process::ProcessModel>{"fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7"};

const State::AddressAccessor lightAddress{State::Address{"light", {"intensity"}}};

struct Patch
{
  const score::GUIApplicationContext& app;
  score::Document* doc{};
  Scenario::IntervalModel* itv{};

  explicit Patch(const score::GUIApplicationContext& a)
      : app{a}
  {
    doc = score::test::new_document(app);
    REQUIRE(doc);
    itv = &model().baseInterval();
  }

  Scenario::ScenarioDocumentModel& model() const
  {
    return static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
  }

  Process::ProcessModel& add(const std::string& name, UuidKey<Process::ProcessModel> key)
  {
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
    auto* p = m.createProcessInNewSlot(*itv, key, {});
    REQUIRE(p);
    m.commit();
    p->metadata().setName(QString::fromStdString(name));
    return *p;
  }

  //! By name: undoing a removal brings back new objects
  Process::ProcessModel& proc(const std::string& name) const
  {
    for(auto& p : itv->processes)
      if(p.metadata().getName().toStdString() == name)
        return p;
    FAIL("no process " << name);
    throw;
  }

  void addAll(std::initializer_list<const char*> names, UuidKey<Process::ProcessModel> key)
  {
    for(auto n : names)
      add(n, key);
  }

  void connect(const std::string& a, const std::string& b)
  {
    auto& dm = model();
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        dm, getStrongId(dm.cables), Process::CableType::ImmediateGlutton,
        *proc(a).outlets()[0], *proc(b).inlets()[0]);
  }

  //! Every cable of the document, as "source->sink" by process name, with the
  //! port index when it is not the first one.
  std::multiset<std::string> cables() const
  {
    auto name = [](const Process::Port& port) {
      auto proc = qobject_cast<const Process::ProcessModel*>(port.parent());
      std::string n = proc ? proc->metadata().getName().toStdString() : "?";
      if(proc)
      {
        auto& ins = proc->inlets();
        auto& outs = proc->outlets();
        auto i = std::find(ins.begin(), ins.end(), &port) - ins.begin();
        auto o = std::find(outs.begin(), outs.end(), &port) - outs.begin();
        if(i < (long)ins.size() && i != 0)
          n += ":" + std::to_string(i);
        if(o < (long)outs.size() && o != 0)
          n += ":" + std::to_string(o);
      }
      return n;
    };
    std::multiset<std::string> s;
    for(auto& c : model().cables)
      s.insert(
          name(c.source().find(doc->context())) + "->"
          + name(c.sink().find(doc->context())));
    return s;
  }

  bool contains(const std::string& name) const
  {
    for(auto& p : itv->processes)
      if(p.metadata().getName().toStdString() == name)
        return true;
    return false;
  }

  //! Selects the processes and triggers the action, as the user does
  template <typename Action>
  void remove(std::initializer_list<const char*> names)
  {
    Selection sel;
    for(auto n : names)
      sel.append(&proc(n));
    doc->context().selectionStack.pushNewSelection(sel);
    QApplication::processEvents();
    app.actions.action<Action>().action()->trigger();
    QApplication::processEvents();
  }

  ~Patch()
  {
    doc->context().selectionStack.deselect();
    QApplication::processEvents();
    QApplication::processEvents();
  }
};

using Cables = std::multiset<std::string>;
using Delete = Actions::RemoveElements;
using ShiftDelete = Actions::RemoveElementsAndCables;

//! Removes with the action, checks the cables after, then that one undo step
//! brings back exactly the cables there were.
template <typename Action>
void checkRemoval(
    Patch& p, std::initializer_list<const char*> removed, const Cables& expected)
{
  auto& stack = p.doc->commandStack();
  const auto before = p.cables();
  const int index = stack.currentIndex();

  p.remove<Action>(removed);

  for(auto n : removed)
    CHECK(!p.contains(n));
  CHECK(p.cables() == expected);
  CHECK(stack.currentIndex() == index + 1);

  stack.undo();
  QApplication::processEvents();
  for(auto n : removed)
    CHECK(p.contains(n));
  CHECK(p.cables() == before);
  CHECK(stack.currentIndex() == index);
}
}

TEST_CASE("The remove actions keep their saved shortcut key", "[integration][remove]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto& del = app.actions.action<Delete>();
    auto& shiftDel = app.actions.action<ShiftDelete>();
    CHECK(shiftDel.key() == score::ActionKey{"RemoveElementsKeepLinked"});
    CHECK(del.key() == score::ActionKey{"RemoveElements"});
    CHECK(shiftDel.action()->text() == QObject::tr("Remove elements and their &cables"));
    CHECK(del.action()->text() == QObject::tr("&Remove elements"));
  });
}

TEST_CASE(
    "Removing several processes of a chain follows the action",
    "[integration][remove][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    SECTION("Consecutive processes go as one sub-chain")
    {
      Patch p{app};
      p.addAll({"A", "B", "C", "D"}, floatKey);
      p.connect("A", "B");
      p.connect("B", "C");
      p.connect("C", "D");
      checkRemoval<Delete>(p, {"B", "C"}, Cables{"A->D"});
      checkRemoval<ShiftDelete>(p, {"B", "C"}, Cables{});
      // The order of the selection does not matter
      checkRemoval<Delete>(p, {"C", "B"}, Cables{"A->D"});
    }

    SECTION("The last two processes of a chain")
    {
      Patch p{app};
      p.addAll({"A", "B", "C"}, floatKey);
      p.connect("A", "B");
      p.connect("B", "C");
      checkRemoval<Delete>(p, {"B", "C"}, Cables{});
    }

    SECTION("Processes apart in a chain each relink")
    {
      Patch p{app};
      p.addAll({"A", "B", "C", "D", "E"}, floatKey);
      p.connect("A", "B");
      p.connect("B", "C");
      p.connect("C", "D");
      p.connect("D", "E");
      checkRemoval<Delete>(p, {"B", "D"}, Cables{"A->C", "C->E"});
      checkRemoval<ShiftDelete>(p, {"B", "D"}, Cables{});
    }

    SECTION("Fan-in and fan-out around a sub-chain")
    {
      Patch p{app};
      p.addAll({"A1", "A2", "B", "C", "D1", "D2"}, floatKey);
      p.connect("A1", "B");
      p.connect("A2", "B");
      p.connect("B", "C");
      p.connect("C", "D1");
      p.connect("C", "D2");
      checkRemoval<Delete>(
          p, {"B", "C"}, Cables{"A1->D1", "A1->D2", "A2->D1", "A2->D2"});
      checkRemoval<ShiftDelete>(p, {"B", "C"}, Cables{});
    }

    SECTION("Fan-in inside the sub-chain")
    {
      Patch p{app};
      p.addAll({"A1", "A2", "B", "C", "D"}, floatKey);
      p.connect("A1", "B");
      p.connect("B", "C");
      p.connect("A2", "C");
      p.connect("C", "D");
      checkRemoval<Delete>(p, {"B", "C"}, Cables{"A1->D", "A2->D"});
    }
  });
}

TEST_CASE("Delete does not duplicate a cable that exists", "[integration][remove][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    Patch p{app};
    p.addAll({"A", "B", "C"}, floatKey);
    p.connect("A", "B");
    p.connect("B", "C");
    p.connect("A", "C");
    checkRemoval<Delete>(p, {"B"}, Cables{"A->C"});
  });
}

TEST_CASE(
    "A control chain's tail hands its address back only with Delete",
    "[integration][remove][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    // LFO -> Mapping -> /light/intensity
    Patch p{app};
    auto& lfo = p.add("LFO", floatKey);
    auto& mapping = p.add("Mapping", floatKey);
    p.connect("LFO", "Mapping");
    mapping.outlets()[0]->setAddress(lightAddress);
    auto& lfoOut = *lfo.outlets()[0];

    p.remove<ShiftDelete>({"Mapping"});
    CHECK(!p.contains("Mapping"));
    CHECK(lfoOut.address() == State::AddressAccessor{});
    p.doc->commandStack().undo();

    p.remove<Delete>({"Mapping"});
    CHECK(!p.contains("Mapping"));
    CHECK(lfoOut.address() == lightAddress);
    p.doc->commandStack().undo();
    CHECK(lfoOut.address() == State::AddressAccessor{});
  });
}

TEST_CASE(
    "An audio tail hands its propagation back with either action",
    "[integration][remove][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto audio = [](Process::ProcessModel& p) -> Process::AudioOutlet& {
      auto* out = qobject_cast<Process::AudioOutlet*>(p.outlets()[0]);
      REQUIRE(out);
      return *out;
    };

    SECTION("The last two effects of a chain")
    {
      Patch p{app};
      p.addAll({"A", "B", "C"}, mergerKey);
      p.connect("A", "B");
      p.connect("B", "C");
      audio(p.proc("C")).setPropagate(true);
      auto& a = audio(p.proc("A"));
      REQUIRE(!a.propagate());

      p.remove<ShiftDelete>({"B", "C"});
      CHECK(a.propagate());
      CHECK(a.cables().empty());
      p.doc->commandStack().undo();
      CHECK(!a.propagate());

      p.remove<Delete>({"C", "B"});
      CHECK(a.propagate());
      p.doc->commandStack().undo();
      CHECK(!a.propagate());
      CHECK(p.cables() == Cables{"A->B", "B->C"});
    }

    SECTION("Every source left without a cable becomes audible")
    {
      // A1 and A2 mix into C, A1 also feeds D: A1 stays heard through D.
      Patch p{app};
      p.addAll({"A1", "A2", "A3", "C", "D"}, mergerKey);
      p.connect("A1", "C");
      p.connect("A2", "C");
      p.connect("A3", "C");
      p.connect("A1", "D");
      audio(p.proc("C")).setPropagate(true);

      for(bool shift : {false, true})
      {
        if(shift)
          p.remove<ShiftDelete>({"C"});
        else
          p.remove<Delete>({"C"});
        CHECK(!audio(p.proc("A1")).propagate());
        CHECK(audio(p.proc("A2")).propagate());
        CHECK(audio(p.proc("A3")).propagate());
        p.doc->commandStack().undo();
        CHECK(!audio(p.proc("A2")).propagate());
        CHECK(!audio(p.proc("A3")).propagate());
      }
    }
  });
}
