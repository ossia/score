// Fold state lives on the process, not on the node item that draws it.
//
// That is what lets it survive a save/reload, an undo, and -- the case this was
// written for -- a preset dropped onto an open node: the replacement keeps the
// node open however many ports it brings, instead of falling back to the
// "too many ports to draw" heuristic and closing itself.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Effect/EffectLayer.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortVisibility.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Application/Drops/DropOnCable.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QPointer>
#include <QMimeData>
#include <QPointF>

#include <catch2/catch_test_macros.hpp>

namespace
{
// The current "LFO": a handful of controls, well under the auto-fold threshold.
const auto lfo_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("1e17e479-3513-44c8-a8a7-017be9f6ac8a"));

Process::ProcessModel& add_lfo(score::Document& doc, Scenario::IntervalModel& interval)
{
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, lfo_key, QString{}, QPointF{});

  Process::ProcessModel* added = nullptr;
  for(auto& p : interval.processes)
    if(p.concreteKey() == lfo_key)
      added = &p;

  REQUIRE(added != nullptr);
  return *added;
}

Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}
}

TEST_CASE("Auto fold mode keeps the port-count heuristic", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));

    // Untouched processes keep the heuristic behaviour: the LFO's handful of
    // controls sits under the threshold, so Auto means open.
    CHECK(lfo.foldMode() == Process::FoldMode::Auto);
    REQUIRE(std::ssize(lfo.inlets()) <= Process::MaxUnpaginatedControls);
    CHECK_FALSE(lfo.folded());

    // An explicit choice overrides the heuristic in both directions.
    lfo.setFoldMode(Process::FoldMode::Folded);
    CHECK(lfo.folded());
    lfo.setFoldMode(Process::FoldMode::Unfolded);
    CHECK(!lfo.folded());
  });
}

TEST_CASE("Fold mode survives a save and reload", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));

    lfo.setFoldMode(Process::FoldMode::Folded);

    auto* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    Process::ProcessModel* p = nullptr;
    for(auto& proc : base_interval(*reloaded).processes)
      if(proc.concreteKey() == lfo_key)
        p = &proc;

    REQUIRE(p != nullptr);
    CHECK(p->foldMode() == Process::FoldMode::Folded);
    CHECK(p->folded());
  });
}

TEST_CASE(
    "A document written before fold state existed loads as Auto", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));
    lfo.setFoldMode(Process::FoldMode::Folded);

    JSONReader writer;
    doc->saveAsJson(writer);
    QByteArray json{writer.buffer.GetString(), int(writer.buffer.GetSize())};
    REQUIRE(json.contains("FoldMode"));

    // What every .score written before this feature looks like: the key is
    // simply absent, and those must come back as Auto rather than inheriting
    // whatever the member happened to hold.
    json.replace("\"FoldMode\"", "\"FoldModeWasNotAThing\"");

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* older = ctx.docManager.loadDocument(
        ctx, QStringLiteral("older"), json, JSONObject::type(), *delegates.begin());
    REQUIRE(older != nullptr);

    Process::ProcessModel* p = nullptr;
    for(auto& proc : base_interval(*older).processes)
      if(proc.concreteKey() == lfo_key)
        p = &proc;

    REQUIRE(p != nullptr);
    CHECK(p->foldMode() == Process::FoldMode::Auto);
  });
}

namespace
{
//! Everything DropOnNode needs: it reads the process off a NodeItem, and the
//! item has to live in a scene for its children to be built.
struct DropFixture
{
  score::Document& doc;
  Scenario::IntervalModel& interval;

  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx{doc.context(), dfm, fd};

  QGraphicsScene scene;
  QGraphicsRectItem root{QRectF{0., 0., 1000., 1000.}};

  DropFixture(score::Document& d, Scenario::IntervalModel& itv)
      : doc{d}
      , interval{itv}
  {
    scene.addItem(&root);
  }

  ~DropFixture() { scene.removeItem(&root); }

  QPointer<Process::NodeItem> nodeFor(Process::ProcessModel& p)
  {
    auto* item = new Process::NodeItem{p, pctx, TimeVal::fromMsecs(1000.), &root};

    // NodalIntervalView::on_processRemoving does this in the real view. Without
    // it the item outlives the process the drop removes, and PortItem's own
    // "Port destroyed before its item" assert fires.
    QObject::connect(
        &p, &IdentifiedObjectAbstract::identified_object_destroying, &p,
        [item = QPointer<Process::NodeItem>{item}] { delete item.data(); });

    QCoreApplication::processEvents();
    return item;
  }

  //! Drop `preset` onto `on`, the way NodalIntervalView::on_dropOnNode does.
  //! drop() defers the real work, so the event loop has to be pumped after.
  void dropPreset(Process::ProcessModel& on, const Process::Preset& preset)
  {
    droppedKey = on.concreteKey();
    QPointer<Process::NodeItem> item = nodeFor(on);
    auto& sm = static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());

    Scenario::DropOnNode dropper{*item, sm, pctx};
    QMimeData mime;
    mime.setData(score::mime::processpreset(), preset.toJson());
    dropper.drop(mime);

    QCoreApplication::processEvents();

    // Normally gone with its process by now; only left over if the drop
    // decided not to replace anything.
    delete item.data();
  }

  //! Drop `dragged`, held by its folder icon, onto `on`: what
  //! EffectLayer's drag puts in the mime data.
  //! `pathOf` stands for a process with the same identifiers as `dragged`,
  //! as two documents opened from one file have.
  void dropNode(
      Process::ProcessModel& on, const Process::ProcessModel& dragged,
      const Process::ProcessModel* pathOf = nullptr)
  {
    QByteArray data;
    {
      JSONReader r;
      r.stream.StartObject();
      Process::copyProcess(r, dragged);
      r.obj["Path"] = score::IDocument::path(pathOf ? *pathOf : dragged);
      r.stream.EndObject();
      data = r.toByteArray();
    }
    QPointer<Process::NodeItem> item = nodeFor(on);
    auto& sm = static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());

    Scenario::DropOnNode dropper{*item, sm, pctx};
    QMimeData mime;
    mime.setData(score::mime::layerdata(), data);
    dropper.drop(mime);

    QCoreApplication::processEvents();
    delete item.data();
  }

  //! The process the drop put in `previous`'s place.
  //!
  //! Not simply "the one that is not previous": the base interval also holds
  //! the document's root Scenario, and IdContainer<Ordered>::insert appends --
  //! so that save -> load -> save reaches a fixed point -- which puts the
  //! Scenario first. That test would return it: a process whose foldMode is
  //! Auto, satisfying the folded case below while checking nothing it means
  //! to check.
  //!
  //! Matched on the dropped key, captured in dropPreset() while the process is
  //! still alive: `previous` is a dangling reference by the time this is
  //! called, since the drop removed and destroyed it. Address comparison
  //! against it is fine; calling a virtual on it is not.
  Process::ProcessModel* replacementOf(const Process::ProcessModel& previous)
  {
    for(auto& p : interval.processes)
      if(&p != &previous && p.concreteKey() == droppedKey)
        return &p;
    return nullptr;
  }

  UuidKey<Process::ProcessModel> droppedKey{};
};
}

TEST_CASE(
    "Dropping a preset on an open node keeps the replacement open",
    "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = base_interval(*doc);
    auto& old = add_lfo(*doc, interval);

    // The node being dropped onto is open -- here by the heuristic, which is
    // the case that used to be lost across the replacement.
    REQUIRE(!old.folded());
    REQUIRE(old.foldMode() == Process::FoldMode::Auto);

    const auto oldId = old.id();
    const Process::Preset preset = old.savePreset();

    DropFixture fx{*doc, interval};
    fx.dropPreset(old, preset);

    // The drop must actually have replaced the process rather than loading the
    // preset into it: the whole bug only exists on the replacement path.
    auto* replacement = fx.replacementOf(old);
    REQUIRE(replacement != nullptr);
    REQUIRE(replacement->id() != oldId);

    CHECK(replacement->foldMode() == Process::FoldMode::Unfolded);
    CHECK(!replacement->folded());
  });
}

TEST_CASE(
    "Dropping a preset on a folded node leaves the replacement to the heuristic",
    "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = base_interval(*doc);
    auto& old = add_lfo(*doc, interval);

    // One-directional: a node the user had closed must not force the
    // replacement closed, it just does not have an opinion to carry over.
    old.setFoldMode(Process::FoldMode::Folded);
    REQUIRE(old.folded());

    const Process::Preset preset = old.savePreset();

    DropFixture fx{*doc, interval};
    fx.dropPreset(old, preset);

    auto* replacement = fx.replacementOf(old);
    REQUIRE(replacement != nullptr);
    CHECK(replacement->foldMode() == Process::FoldMode::Auto);
  });
}

TEST_CASE(
    "A node dragged onto another replaces it with a copy, in its own document only",
    "[integration][drop][node]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = base_interval(*doc);
    auto& dragged = add_lfo(*doc, interval);
    const auto draggedId = dragged.id();
    {
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
          interval, lfo_key, QString{}, QPointF{300., 200.});
    }
    Process::ProcessModel* target{};
    for(auto& p : interval.processes)
      if(p.concreteKey() == lfo_key && p.id() != draggedId)
        target = &p;
    REQUIRE(target);
    const auto targetId = target->id();
    const auto targetPos = target->position();
    dragged.metadata().setName("dragged");
    const auto count = interval.processes.size();

    {
      DropFixture fx{*doc, interval};
      fx.dropNode(*target, dragged);
    }

    // The target is gone, a copy of the dragged node stands where it was, and
    // the dragged node is still there
    CHECK(interval.processes.size() == count);
    CHECK(interval.processes.find(targetId) == interval.processes.end());
    REQUIRE(interval.processes.find(draggedId) != interval.processes.end());
    Process::ProcessModel* copy{};
    for(auto& p : interval.processes)
      if(p.concreteKey() == lfo_key && p.id() != draggedId)
        copy = &p;
    REQUIRE(copy);
    CHECK(copy->position() == targetPos);

    // One step to undo
    doc->commandStack().undo();
    CHECK(interval.processes.find(targetId) != interval.processes.end());
    CHECK(interval.processes.size() == count);

    // Dragged from another document: its path means nothing here, even when a
    // process sits at the same path. Nothing is replaced.
    score::Document* other = score::test::new_document(ctx);
    REQUIRE(other != nullptr);
    auto& otherInterval = base_interval(*other);
    auto& otherDragged = add_lfo(*other, otherInterval);
    auto& there = *interval.processes.find(targetId);
    const int index = doc->commandStack().currentIndex();
    {
      DropFixture fx{*doc, interval};
      fx.dropNode(there, otherDragged, &*interval.processes.find(draggedId));
    }
    CHECK(doc->commandStack().currentIndex() == index);
    CHECK(interval.processes.find(targetId) != interval.processes.end());
    CHECK(interval.processes.size() == count);
  });
}
