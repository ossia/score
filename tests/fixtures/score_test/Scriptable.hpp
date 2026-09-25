#pragma once

// Published (scriptable) objects of a test document: their parameters in the
// local tree, writes and state recalls coming from another thread, the waits
// their queued updates need, and replays of commands from their saved form.

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/command/CommandData.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <score_test/Events.hpp>

#include <memory>
#include <thread>

namespace score::test
{

//! Delivers what is posted, deferred deletes included, including what the
//! delivered events post in turn.
inline void process_events()
{
  for(int i = 0; i < 10; i++)
  {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::AllEvents);
  }
}

//! Runs the event loop past the quiet time that ends a burst of writes into a
//! published control (250 ms in LocalTree's ScriptableProcessComponent), for a
//! check that a burst committed nothing.
inline void wait_past_burst()
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < 400)
  {
    QThread::msleep(1);
    process_events();
  }
}

//! The control inlet of \p p named \p name, or nullptr.
inline Process::ControlInlet* find_control(const Process::ProcessModel& p, const QString& name)
{
  for(auto* inlet : p.inlets())
    if(auto* ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return ctl;
  return nullptr;
}

//! The start state of the document's base scenario.
inline Scenario::StateModel& start_state(score::Document& doc)
{
  return doc.context().model<Scenario::ScenarioDocumentModel>().baseScenario().startState();
}

//! The end state of the document's base scenario, which stopping plays.
inline Scenario::StateModel& end_state(score::Document& doc)
{
  return doc.context().model<Scenario::ScenarioDocumentModel>().baseScenario().endState();
}

inline State::MessageList messages(const Scenario::StateModel& s)
{
  return Process::flatten(s.messages().rootNode());
}

//! The parameter published at \p a in the document's local tree, or nullptr.
inline ossia::net::parameter_base*
find_published(const score::DocumentContext& ctx, const State::Address& a)
{
  auto& dev = ctx.plugin<LocalTree::DocumentPlugin>().device();
  auto n = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  return n ? n->get_parameter() : nullptr;
}

//! The parameter published at \p a; fails the test when there is none.
inline ossia::net::parameter_base&
published(const score::DocumentContext& ctx, const State::Address& a)
{
  auto p = find_published(ctx, a);
  REQUIRE(p);
  return *p;
}

//! Pushes \p v from another thread, as a script, a remote client or the
//! execution does, then delivers what that posted to the GUI thread.
inline void push_from_thread(ossia::net::parameter_base& p, const ossia::value& v)
{
  std::thread{[&] { p.push_value(v); }}.join();
  process_events();
}

//! Sends the messages of \p state as the execution does: addresses resolved
//! against the local tree first, the messages then launched from another thread.
inline void launch_state(const score::DocumentContext& ctx, const Scenario::StateModel& state)
{
  ossia::execution_state st;
  st.register_device(&ctx.plugin<LocalTree::DocumentPlugin>().device());
  st.apply_device_changes();
  auto s = Engine::score_to_ossia::state(state, st);
  std::thread{[&] { s.launch(); }}.join();
}

//! Replaces the last command of \p doc by a copy made from its saved form, as
//! a crash restore does.
inline void replay_last_command(score::Document& doc)
{
  auto& stack = doc.commandStack();
  REQUIRE(stack.canUndo());
  const score::CommandData data{*stack.command(stack.currentIndex() - 1)};
  stack.undo();
  process_events();
  std::unique_ptr<score::Command> copy{
      doc.context().app.components.instantiateUndoCommand(data)};
  REQUIRE(copy);
  copy->redo(doc.context());
  process_events();
  stack.push(copy.release());
}

}
