// A Faust program edited so that one of its ports is rebuilt as another
// object (a slider that becomes a knob) keeps that port's cables: the cable
// follows the new port, and no port lists a cable the document does not have.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Faust/Commands.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString faust_uuid = QStringLiteral("5354c61a-1649-4f59-b952-5c2f1b79c1bd");

void requireConsistentCables(
    const Scenario::ScenarioDocumentModel& sm,
    std::initializer_list<const Process::ProcessModel*> procs,
    const score::DocumentContext& ctx)
{
  for(auto* proc : procs)
  {
    auto check = [&](const Process::Port& port) {
      for(auto& path : port.cables())
      {
        INFO("port " << port.name().toStdString());
        auto* c = path.try_find(ctx);
        REQUIRE(c != nullptr);
        const bool is_end = c->source().try_find(ctx) == &port
                            || c->sink().try_find(ctx) == &port;
        CHECK(is_end);
      }
    };
    for(auto* p : proc->inlets())
      check(*p);
    for(auto* p : proc->outlets())
      check(*p);
  }
  for(auto& c : sm.cables)
  {
    auto* src = c.source().try_find(ctx);
    auto* snk = c.sink().try_find(ctx);
    REQUIRE(src);
    REQUIRE(snk);
    auto lists = [&](const Process::Port& p) {
      return ossia::any_of(p.cables(), [&](auto& path) {
        return path.unsafePath().vec().back().id() == c.id().val();
      });
    };
    CHECK(lists(*src));
    CHECK(lists(*snk));
  }
}
}

TEST_CASE(
    "A Faust port rebuilt by a program edit keeps its cables",
    "[integration][faust][cable]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* src = score::test::add_process(
        *doc, faust_uuid, QStringLiteral("process = hbargraph(\"level\", 0, 1);"));
    if(!src)
      SKIP("the Faust plug-in is not in this build");
    auto* fx = score::test::add_process(
        *doc, faust_uuid,
        QStringLiteral("process = _ * hslider(\"gain\", 0.5, 0, 1, 0.01);"));
    REQUIRE(fx);
    auto& faust = static_cast<Faust::FaustEffectModel&>(*fx);

    REQUIRE(src->outlets().size() == 2);
    REQUIRE(fx->inlets().size() == 2);
    const Process::Outlet& level = *src->outlets()[1];
    const auto* slider = fx->inlets()[1];
    REQUIRE(level.type() == slider->type());

    auto& dc = doc->context();
    auto& sm = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<> disp{dc.commandStack};

    const auto cable_id = getStrongId(sm.cables);
    disp.submit(new Dataflow::CreateCable{
        sm, cable_id, Process::CableType::ImmediateGlutton, level, *slider});
    REQUIRE(sm.cables.find(cable_id) != sm.cables.end());

    // The same control, now drawn as a knob: a new port object with another id.
    disp.submit(new Faust::EditScript{
        faust,
        QStringLiteral("process = _ * hslider(\"gain[style:knob]\", 0.5, 0, 1, 0.01);"),
        dc});
    REQUIRE(fx->inlets().size() == 2);
    const auto* knob = fx->inlets()[1];
    REQUIRE(knob != slider);
    REQUIRE(knob->id() != slider->id());

    requireConsistentCables(sm, {src, fx}, dc);
    {
      auto it = sm.cables.find(cable_id);
      REQUIRE(it != sm.cables.end());
      CHECK(it->source().try_find(dc) == &level);
      CHECK(it->sink().try_find(dc) == knob);
    }

    // A new cable to the knob gets a fresh id and lands on both ends.
    const auto second_id = getStrongId(sm.cables);
    disp.submit(new Dataflow::CreateCable{
        sm, second_id, Process::CableType::ImmediateGlutton, *src->outlets()[0],
        *fx->inlets()[0]});
    disp.submit(new Dataflow::CreateCable{
        sm, getStrongId(sm.cables), Process::CableType::ImmediateGlutton, level, *knob});
    requireConsistentCables(sm, {src, fx}, dc);
    CHECK(knob->cables().size() == 2);

    // Back to before the program edit: the slider has its cable again.
    auto& stack = doc->commandStack();
    stack.undo();
    stack.undo();
    stack.undo();
    REQUIRE(fx->inlets().size() == 2);
    requireConsistentCables(sm, {src, fx}, dc);
    {
      auto it = sm.cables.find(cable_id);
      REQUIRE(it != sm.cables.end());
      CHECK(it->sink().try_find(dc) == fx->inlets()[1]);
      CHECK(fx->inlets()[1]->cables().size() == 1);
    }

    stack.redo();
    requireConsistentCables(sm, {src, fx}, dc);
    {
      auto it = sm.cables.find(cable_id);
      REQUIRE(it != sm.cables.end());
      CHECK(it->sink().try_find(dc) == fx->inlets()[1]);
    }
  });
}
