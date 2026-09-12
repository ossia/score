// SPDX-License-Identifier: GPL-3.0-or-later
//
// An add-on that gains or loses a port makes every saved instance of it
// mismatch the spec. score rebuilds the ports from the new spec and restores
// what it saved from the old ones -- and that restore was dropping every
// control value, so a document came back with the whole process at its
// defaults. A granular synth lost its soundfile, its position, its pitch and
// the rest, in silence.
//
// loadData only restores a value when asked, and the rebuild path was asking
// for cables and addresses only. The two callers that legitimately want the
// new values (loading a preset, editing a script) pass no flag and are
// unaffected; this one is the version-drift path, where the saved values are
// the only thing left worth keeping.

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "rebuilding a process's ports keeps its control values",
    "[integration][ports]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto* doc = score::test::project_document(ctx, dir.path());
    REQUIRE(doc != nullptr);

    // The LFO: a handful of plain float controls, and nothing about the bug
    // was specific to a port type.
    auto* proc = score::test::add_process(
        *doc, QStringLiteral("1e17e479-3513-44c8-a8a7-017be9f6ac8a"), QString{});
    REQUIRE(proc != nullptr);

    // Any control will do: the bug was in the restore, not in a port type.
    Process::ControlInlet* ctl{};
    for(auto* inl : proc->inlets())
      if(auto* c = qobject_cast<Process::ControlInlet*>(inl))
      {
        ctl = c;
        break;
      }
    REQUIRE(ctl != nullptr);

    const ossia::value saved_value{0.625};
    ctl->setValue(saved_value);

    std::vector<Dataflow::SavedPort> in, out;
    for(auto* port : proc->inlets())
      in.push_back({port->name(), port->type(), port->saveData()});
    for(auto* port : proc->outlets())
      out.push_back({port->name(), port->type(), port->saveData()});

    // What the rebuild leaves behind: ports of the right shape holding
    // whatever the current spec defaults to.
    ctl->setValue(ossia::value{0.125});
    REQUIRE(ctl->value() == ossia::value{0.125});

    Dataflow::reloadPortsInNewProcess(in, out, *proc);

    CHECK(ctl->value() == saved_value);
  });
}
