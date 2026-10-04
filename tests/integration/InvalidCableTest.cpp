// A cable never lands on a port of another type: when the ports changed since
// it was saved, it is not restored, re-created by a replayed command, pasted
// or loaded -- that breaks the execution afterwards.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");
const QString envelope_uuid = QStringLiteral("95F44151-13EF-4537-8189-0CC243341269");

Process::CableData data(const Process::Port& src, const Process::Port& snk)
{
  return Process::CableData{Process::CableType::ImmediateGlutton, src, snk};
}
}

TEST_CASE("a cable between ports of different types is not restored", "[integration][cable]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* lfo = score::test::add_process(*doc, lfo_uuid, {});
    auto* lfo2 = score::test::add_process(*doc, lfo_uuid, {});
    auto* env = score::test::add_process(*doc, envelope_uuid, {});
    if(!lfo || !lfo2 || !env)
      SKIP("Fx not built");
    REQUIRE(env->inlets()[0]->type() == Process::PortType::Audio);
    REQUIRE(lfo->outlets()[0]->type() == Process::PortType::Message);

    auto& dc = doc->context();
    auto& sm = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);

    // What an undo would restore after the sink was rebuilt as an audio port.
    Dataflow::SerializedCables cables{
        {Id<Process::Cable>{4242}, data(*lfo->outlets()[0], *env->inlets()[0])},
        {Id<Process::Cable>{4243}, data(*lfo->outlets()[0], *lfo2->inlets()[0])}};
    const auto restored = Dataflow::restoreCables(cables, dc);
    REQUIRE(restored.size() == 1);
    CHECK(restored[0]->id() == Id<Process::Cable>{4243});
    CHECK(sm.cables.find(Id<Process::Cable>{4242}) == sm.cables.end());
    // And none of the ports still lists it.
    CHECK(lfo->outlets()[0]->cables().size() == 1);
    CHECK(env->inlets()[0]->cables().empty());

    // A replayed command whose ports no longer match connects nothing.
    CommandDispatcher<>{dc.commandStack}.submit(new Dataflow::CreateCable{
        sm, Id<Process::Cable>{4244}, Process::CableType::ImmediateGlutton,
        *lfo->outlets()[0], *env->inlets()[0]});
    CHECK(sm.cables.find(Id<Process::Cable>{4244}) == sm.cables.end());
    CHECK(env->inlets()[0]->cables().empty());
  });
}
