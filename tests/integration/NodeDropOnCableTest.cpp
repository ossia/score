// A node dragged onto a cable goes in it only if it has no cable yet: one
// already wired would have to be cut from its chain, or sit in two at once.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

Process::Cable& connect(
    score::Document& doc, const Process::Port& src, const Process::Port& snk)
{
  auto& sm = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto& c = m.createCable(sm, src, snk, Process::CableType::ImmediateGlutton);
  m.commit();
  return c;
}
}

TEST_CASE("only a node with no cable may be dropped on a cable", "[integration][nodal][cable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, lfo_uuid, {});
    if(!a)
      SKIP("LFO not built");
    auto* b = score::test::add_process(*doc, lfo_uuid, {});
    auto* c = score::test::add_process(*doc, lfo_uuid, {});
    REQUIRE(b);
    REQUIRE(c);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    auto& ab = connect(*doc, *a->outlets()[0], *b->inlets()[0]);
    CHECK(Scenario::canInsertProcessInCable(pctx, *c, ab));

    // C is wired to B already: it stays where it is.
    connect(*doc, *c->outlets()[0], *b->inlets()[1]);
    CHECK_FALSE(Scenario::canInsertProcessInCable(pctx, *c, ab));
  });
}
