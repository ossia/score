// Nodes of a Nodal process are copied as processes anywhere are, and pasted
// back in the Nodal process, with the cables between them.

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Media/Step/Model.hpp>
#include <Nodal/Commands.hpp>
#include <Nodal/Process.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QMimeData>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>

TEST_CASE(
    "Nodes copied in a Nodal process paste back in it, with their cables", "[nodal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);

    // A box with a Nodal process holding an automation driving a step sequencer.
    Nodal::Model* nodal{};
    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      box = &m.createBox(
          scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(5000), 0.2);
      nodal = qobject_cast<Nodal::Model*>(m.createProcessInNewSlot(
          *box, UuidKey<Process::ProcessModel>{"f5678806-c431-45c5-ae3a-fae5183380fb"},
          {}, QPointF{}));
      m.commit();
    }
    REQUIRE(nodal);
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{0., 0.}, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(),
        {}, Id<Process::ProcessModel>{1}});
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{300., 0.}, Metadata<ConcreteKey_k, Media::Step::Model>::get(), {},
        Id<Process::ProcessModel>{2}});
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{600., 0.}, Metadata<ConcreteKey_k, Media::Step::Model>::get(), {},
        Id<Process::ProcessModel>{3}});
    REQUIRE(nodal->nodes.size() == 3);
    auto& autom = nodal->nodes.at(Id<Process::ProcessModel>{1});
    auto& step = nodal->nodes.at(Id<Process::ProcessModel>{2});
    auto& last = nodal->nodes.at(Id<Process::ProcessModel>{3});
    {
      // Two cables: they take two identifiers on paste.
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      m.createCable(
          document, *autom.outlets().front(), *step.inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.createCable(
          document, *step.outlets().front(), *last.inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }
    const auto cables_before = document.cables.size();
    const auto box_processes = box->processes.size();

    // Copied as Copy does it.
    doc->context().selectionStack.pushNewSelection(Selection{&autom, &step, &last});
    JSONReader r;
    bool copied = false;
    for(auto& ed : ctx.interfaces<score::ObjectEditorList>())
      if((copied = ed.copy(r, doc->context().selectionStack.currentSelection(), doc->context())))
        break;
    REQUIRE(copied);
    QMimeData mime;
    mime.setData("text/plain", r.toByteArray());

    // Pasted with a node of it selected: in it, not in its box.
    doc->context().selectionStack.pushNewSelection(Selection{&step});
    bool pasted = false;
    for(auto& ed : ctx.interfaces<score::ObjectEditorList>())
      if((pasted = ed.paste(QPoint{}, nullptr, mime, doc->context())))
        break;
    REQUIRE(pasted);

    CHECK(box->processes.size() == box_processes);
    REQUIRE(nodal->nodes.size() == 6);
    CHECK(document.cables.size() == cables_before + 2);

    const Process::ProcessModel* new_autom{};
    const Process::ProcessModel* new_step{};
    for(auto& n : nodal->nodes)
    {
      if(&n == &autom || &n == &step || &n == &last)
        continue;
      if(qobject_cast<const Automation::ProcessModel*>(&n))
        new_autom = &n;
      else if(n.position() == QPointF{340., 40.})
        new_step = &n;
    }
    REQUIRE(new_autom);
    REQUIRE(new_step);
    CHECK(new_autom->position() == QPointF{40., 40.});
    CHECK(new_step->position() == QPointF{340., 40.});

    // The new cable joins the copies.
    REQUIRE(new_autom->outlets().front()->cables().size() == 1);
    const auto& c = new_autom->outlets().front()->cables().front().find(doc->context());
    CHECK(&c.sink().find(doc->context()) == new_step->inlets().front());

    doc->commandStack().undo();
    CHECK(nodal->nodes.size() == 3);
    CHECK(document.cables.size() == cables_before);
  });
}

#include <Effect/EffectLayer.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>

namespace
{
const UuidKey<Process::ProcessModel> nodal_key{"f5678806-c431-45c5-ae3a-fae5183380fb"};

//! A box holding a Nodal process: an automation driving a step sequencer
//! driving another one.
struct ThreeNodes
{
  explicit ThreeNodes(const score::GUIApplicationContext& ctx, int boxMsecs = 4000)
      : doc{score::test::new_document(ctx)}
  {
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      box = &m.createBox(
          scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(1000 + boxMsecs), 0.2);
      nodal = qobject_cast<Nodal::Model*>(
          m.createProcessInNewSlot(*box, nodal_key, {}, QPointF{}));
      m.commit();
    }
    REQUIRE(nodal);
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{0., 0.}, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(),
        {}, Id<Process::ProcessModel>{1}});
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{300., 0.}, Metadata<ConcreteKey_k, Media::Step::Model>::get(), {},
        Id<Process::ProcessModel>{2}});
    disp.submit(new Nodal::CreateNode{
        *nodal, QPointF{600., 0.}, Metadata<ConcreteKey_k, Media::Step::Model>::get(), {},
        Id<Process::ProcessModel>{3}});
    auto& autom = node(1);
    auto& step = node(2);
    auto& last = node(3);
    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
    m.createCable(
        document(), *autom.outlets().front(), *step.inlets().front(),
        Process::CableType::ImmediateGlutton);
    m.createCable(
        document(), *step.outlets().front(), *last.inlets().front(),
        Process::CableType::ImmediateGlutton);
    m.commit();
  }

  Scenario::ScenarioDocumentModel& document() const
  {
    return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);
  }
  Process::ProcessModel& node(int id) const
  {
    return nodal->nodes.at(Id<Process::ProcessModel>{id});
  }

  //! The three nodes as Copy copies them
  rapidjson::Document copy() const
  {
    JSONReader r;
    r.stream.StartObject();
    Process::copyProcesses(r, {&node(1), &node(2), &node(3)}, doc->context());
    r.stream.EndObject();
    return readJson(r.toByteArray());
  }

  score::Document* doc{};
  Scenario::IntervalModel* box{};
  Nodal::Model* nodal{};
};

//! The editor of that type, asked alone
template <typename Editor>
score::ObjectEditor& editor(const score::GUIApplicationContext& ctx)
{
  for(auto& ed : ctx.interfaces<score::ObjectEditorList>())
    if(ed.concreteKey() == Editor::static_concreteKey())
      return ed;
  FAIL("no such editor");
  throw;
}

std::unique_ptr<QMimeData> mimeOf(const rapidjson::Document& json)
{
  auto mime = std::make_unique<QMimeData>();
  mime->setData("text/plain", jsonToByteArray(json));
  return mime;
}
}

TEST_CASE(
    "A pasted node that cannot be made here is left out, with its cables, and undo "
    "goes back",
    "[nodal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ThreeNodes t{ctx};
    auto json = t.copy();
    // The step sequencer in the middle is of a kind not installed here
    auto& procs = json["Processes"];
    REQUIRE(procs.Size() == 3);
    for(auto& p : procs.GetArray())
      if(p["id"].GetInt() == 2)
        p[score::StringConstant().uuid] = "0b0f0c3c-1d2e-4f5a-8b6c-7d8e9f0a1b2c";

    const auto nodes = t.nodal->nodes.size();
    const auto cables = t.document().cables.size();
    auto cmd = new Nodal::PasteNodes{*t.nodal, json, QPointF{}};
    CHECK(cmd->nodeIds().size() == 2);
    CommandDispatcher<>{t.doc->context().commandStack}.submit(cmd);
    CHECK(t.nodal->nodes.size() == nodes + 2);
    // Both cables had an end on it
    CHECK(t.document().cables.size() == cables);

    t.doc->commandStack().undo();
    CHECK(t.nodal->nodes.size() == nodes);
    t.doc->commandStack().redo();
    CHECK(t.nodal->nodes.size() == nodes + 2);
    t.doc->commandStack().undo();
    CHECK(t.nodal->nodes.size() == nodes);
  });
}

TEST_CASE("Pasting nodes is redone, and reloaded, under the same identifiers", "[nodal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ThreeNodes t{ctx};
    const auto json = t.copy();
    Nodal::PasteNodes cmd{*t.nodal, json, QPointF{}};
    REQUIRE(cmd.nodeIds().size() == 3);
    const auto ids = cmd.nodeIds();
    const auto cables = t.document().cables.size();

    // As the command stack keeps it, reloading a session
    Nodal::PasteNodes reloaded;
    reloaded.deserialize(cmd.serialize());
    CHECK(reloaded.nodeIds() == ids);

    for(int round = 0; round < 2; round++)
    {
      reloaded.redo(t.doc->context());
      for(const auto& id : ids)
        CHECK(t.nodal->nodes.find(id) != t.nodal->nodes.end());
      CHECK(t.document().cables.size() == cables + 2);
      reloaded.undo(t.doc->context());
      CHECK(t.nodal->nodes.size() == 3);
      CHECK(t.document().cables.size() == cables);
    }
  });
}

TEST_CASE("Pasted nodes last as the Nodal process does, and are selected", "[nodal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ThreeNodes from{ctx, 4000};
    ThreeNodes to{ctx, 10000};
    REQUIRE(from.node(1).duration() != to.nodal->duration());

    // In the other document's Nodal process, a node of it selected
    auto& sel = to.doc->context().selectionStack;
    sel.pushNewSelection(Selection{&to.node(1)});
    const auto mime = mimeOf(from.copy());
    REQUIRE(editor<Nodal::NodeEditor>(ctx).paste(QPoint{}, nullptr, *mime, to.doc->context()));
    REQUIRE(to.nodal->nodes.size() == 6);

    for(auto& n : to.nodal->nodes)
      CHECK(n.duration() == to.nodal->duration());
    const auto selected = sel.currentSelection();
    CHECK(selected.size() == 3);
    for(const auto& obj : selected)
      CHECK(qobject_cast<const Process::ProcessModel*>(obj.data())->parent() == to.nodal);

    // Nothing that loads: nothing pasted, nothing to undo
    rapidjson::Document empty;
    empty.SetObject();
    empty.AddMember("Processes", rapidjson::Value(rapidjson::kArrayType), empty.GetAllocator());
    empty.AddMember("Cables", rapidjson::Value(rapidjson::kArrayType), empty.GetAllocator());
    const auto steps = to.doc->commandStack().size();
    CHECK_FALSE(editor<Nodal::NodeEditor>(ctx).paste(
        QPoint{}, nullptr, *mimeOf(empty), to.doc->context()));
    CHECK(to.doc->commandStack().size() == steps);
  });
}

TEST_CASE(
    "Of a box and a Nodal process, the closest one takes what is pasted, whatever "
    "editor is asked first",
    "[nodal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    ThreeNodes t{ctx};
    const auto mime = mimeOf(t.copy());
    auto& sel = t.doc->context().selectionStack;
    auto& nodes = editor<Nodal::NodeEditor>(ctx);
    auto& scenario = editor<Scenario::ScenarioEditor>(ctx);

    // A node selected: the Nodal process takes it, the box does not
    sel.pushNewSelection(Selection{&t.node(2)});
    CHECK_FALSE(scenario.paste(QPoint{}, nullptr, *mime, t.doc->context()));
    CHECK(nodes.paste(QPoint{}, nullptr, *mime, t.doc->context()));
    CHECK(t.nodal->nodes.size() == 6);
    t.doc->commandStack().undo();

    // The box selected: the box takes it, the Nodal process in it does not
    const auto boxProcesses = t.box->processes.size();
    sel.pushNewSelection(Selection{t.box});
    CHECK_FALSE(nodes.paste(QPoint{}, nullptr, *mime, t.doc->context()));
    CHECK(scenario.paste(QPoint{}, nullptr, *mime, t.doc->context()));
    CHECK(t.box->processes.size() == boxProcesses + 3);
    CHECK(t.nodal->nodes.size() == 3);
  });
}
