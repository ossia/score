#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/NodeLayout.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/CommandData.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

namespace
{
Scenario::ScenarioDocumentModel& docModel(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
}

Process::ProcessModel* createMapping(score::Document& doc, Scenario::IntervalModel& itv, QPointF pos)
{
  const Process::ProcessData dat{
      UuidKey<Process::ProcessModel>{"12a5d9b8-823e-4303-99f8-34db37c448b4"},
      QStringLiteral("Mapping"), {}};
  Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto p = m.createProcessInNewSlot(itv, dat, pos);
  m.commit();
  return p;
}

void connect(score::Document& doc, Process::ProcessModel& from, Process::ProcessModel& to)
{
  Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  m.createCable(docModel(doc), *from.outlets().front(), *to.inlets().front(), Process::CableType::ImmediateGlutton);
  m.commit();
}

QRectF footprint(const Process::ProcessModel& p)
{
  return Scenario::estimatedNodeBounds(p).translated(p.position());
}
}

TEST_CASE("auto-layout lays a chain out left to right as one undoable command", "[integration][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = docModel(*doc).baseInterval();
    auto* a = createMapping(*doc, itv, {900., 900.});
    auto* b = createMapping(*doc, itv, {100., 700.});
    auto* c = createMapping(*doc, itv, {500., 100.});
    REQUIRE((a && b && c));
    connect(*doc, *a, *b);
    connect(*doc, *b, *c);

    std::vector<std::pair<const Process::ProcessModel*, QPointF>> before;
    for(const auto& p : itv.processes)
      before.emplace_back(&p, p.position());

    auto& stack = doc->commandStack();
    const int index = stack.currentIndex();
    REQUIRE(Scenario::autoLayoutNodes(itv, {}, doc->context()));
    CHECK(stack.currentIndex() == index + 1);

    CHECK(footprint(*a).right() <= footprint(*b).left());
    CHECK(footprint(*b).right() <= footprint(*c).left());
    for(const auto& p : itv.processes)
      for(const auto& q : itv.processes)
        if(&p != &q)
          CHECK_FALSE(footprint(p).intersects(footprint(q)));

    std::vector<QPointF> laidOut;
    for(const auto& p : itv.processes)
      laidOut.push_back(p.position());

    CHECK_FALSE(Scenario::autoLayoutNodes(itv, {}, doc->context()));
    CHECK(stack.currentIndex() == index + 1);

    stack.undo();
    for(const auto& [p, pos] : before)
      CHECK(p->position() == pos);
    stack.redo();
    std::size_t i = 0;
    for(const auto& p : itv.processes)
      CHECK(p.position() == laidOut[i++]);
  });
}

TEST_CASE("laying out a selection keeps its corner and leaves the rest alone", "[integration][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = docModel(*doc).baseInterval();
    auto* a = createMapping(*doc, itv, {1000., 300.});
    auto* b = createMapping(*doc, itv, {400., 600.});
    auto* other = createMapping(*doc, itv, {3000., 3000.});
    connect(*doc, *a, *b);

    const QRectF corner = footprint(*a).united(footprint(*b));
    const QPointF otherPos = other->position();
    Scenario::autoLayoutNodes(itv, {a, b}, doc->context());

    CHECK(other->position() == otherPos);
    const QRectF after = footprint(*a).united(footprint(*b));
    CHECK(after.topLeft() == corner.topLeft());
    CHECK(footprint(*a).right() <= footprint(*b).left());
  });
}

TEST_CASE("processes of another interval are not laid out", "[integration][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = docModel(*doc).baseInterval();
    auto* a = createMapping(*doc, itv, {500., 500.});
    Scenario::IntervalModel unrelated{Id<Scenario::IntervalModel>{1234}, 0., doc->context(), nullptr};
    CHECK(Scenario::layoutNodes(unrelated, {a}, doc->context()).empty());
  });
}

TEST_CASE("a layout command is replayed from its saved form", "[integration][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = docModel(*doc).baseInterval();
    auto* a = createMapping(*doc, itv, {700., 700.});
    auto* b = createMapping(*doc, itv, {100., 100.});
    connect(*doc, *a, *b);

    REQUIRE(Scenario::autoLayoutNodes(itv, {}, doc->context()));
    const QPointF aPos = a->position(), bPos = b->position();

    auto& stack = doc->commandStack();
    const score::CommandData data{*stack.command(stack.currentIndex() - 1)};
    stack.undo();
    std::unique_ptr<score::Command> copy{
        doc->context().app.components.instantiateUndoCommand(data)};
    REQUIRE(copy);
    CHECK(dynamic_cast<Process::SetNodePositions*>(copy.get()));
    copy->redo(doc->context());
    CHECK(a->position() == aPos);
    CHECK(b->position() == bPos);
  });
}

TEST_CASE("a folded node is estimated at its folded size", "[integration][nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = docModel(*doc).baseInterval();
    auto* a = createMapping(*doc, itv, {100., 100.});
    a->setSize({400., 300.});
    const QRectF unfolded = Scenario::estimatedNodeBounds(*a);
    a->setFoldMode(Process::FoldMode::Folded);
    a->setFoldedSize({90., 30.});
    const QRectF folded = Scenario::estimatedNodeBounds(*a);
    CHECK(folded.width() < unfolded.width());
    CHECK(folded.height() < unfolded.height());
  });
}

