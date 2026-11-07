// The ports in a process inspector and in the audio output panel of the nodal
// view: each port lists its cables, says when those cables keep its address
// from being used, and every edit is one command.

#include <cmath>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Dataflow/AudioOutletItem.hpp>
#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/widgets/QGraphicsCheckBox.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/DoubleSlider.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QTimer>
#include <QToolButton>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/dataflow/port.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// Media::Merger: one audio outlet, eight audio inlets
const QString mergerUuid = QStringLiteral("fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7");
// Gfx::Filter::Model, whose default shader has a texture input
const QString isfUuid = QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b");
// Nodes::LFO v3: control inlets, a value outlet
const QString lfoUuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

const State::AddressAccessor someAddress{State::Address{"audio", {"out", "main"}}};

Scenario::ScenarioDocumentModel& docModel(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
}

Process::Cable&
cable(score::Document& doc, const Process::Port& from, const Process::Port& to)
{
  auto& dm = docModel(doc);
  auto id = getStrongId(dm.cables);
  CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
      dm, id, Process::CableType::ImmediateGlutton, from, to);
  return dm.cables.at(id);
}

//! The deferred refreshes of the inspector widgets, and their deferred selections
void settle()
{
  for(int i = 0; i < 3; i++)
    QApplication::processEvents();
}

template <typename T>
std::vector<T*> visible(QWidget& root, const QString& name)
{
  std::vector<T*> res;
  for(auto w : root.findChildren<T*>(name))
    if(w->isVisibleTo(&root))
      res.push_back(w);
  return res;
}
}

TEST_CASE("cables keep an address from being used", "[integration][inspector]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    auto* b = score::test::add_process(*doc, mergerUuid, {});
    if(!a || !b)
      SKIP("the media plug-in is not built");

    auto& out = *qobject_cast<Process::AudioOutlet*>(a->outlets().front());
    auto& in = *b->inlets().front();
    using S = Process::AddressStatus;

    // Without an address there is nothing to say
    out.setPropagate(false);
    CHECK(S::of(out, dc).kind == S::Used);
    out.setAddress(someAddress);
    in.setAddress(someAddress);
    CHECK(S::of(out, dc).kind == S::Used);
    CHECK(S::of(in, dc).kind == S::Used);

    // A propagated output sends to its parent interval instead
    out.setPropagate(true);
    CHECK(S::of(out, dc).kind == S::Unused);
    CHECK(S::of(out, dc).reason.contains("Propagate"));
    out.setPropagate(false);

    // An immediate glutton cable: the address is used while the other end is idle
    auto& c = cable(*doc, out, in);
    CHECK(S::of(out, dc).kind == S::WhileCablesIdle);
    CHECK(S::of(in, dc).kind == S::WhileCablesIdle);
    CHECK(!S::of(in, dc).reason.isEmpty());

    // Strict or delayed cables only: never used
    c.setType(Process::CableType::ImmediateStrict);
    CHECK(S::of(out, dc).kind == S::Unused);
    CHECK(S::of(in, dc).kind == S::Unused);
    c.setType(Process::CableType::DelayedGlutton);
    CHECK(S::of(in, dc).kind == S::Unused);

    // Removing the cable gives the address back
    CommandDispatcher<>{dc.commandStack}.submit<Dataflow::RemoveCable>(docModel(*doc), c);
    CHECK(S::of(out, dc).kind == S::Used);
    CHECK(S::of(in, dc).kind == S::Used);
  });
}

TEST_CASE("the inspector lists the cables of each port", "[integration][inspector]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    auto* b = score::test::add_process(*doc, mergerUuid, {});
    if(!a || !b)
      SKIP("the media plug-in is not built");
    a->metadata().setName("Source");
    b->metadata().setName("Sink");

    auto& out = *a->outlets().front();
    auto& in = *b->inlets().front();
    in.setAddress(someAddress);

    QWidget window;
    auto list = new Process::PortListWidget{*b, dc, &window};
    window.show();
    settle();

    // Not connected yet: no cable row, no indicator
    CHECK(visible<QWidget>(*list, "PortCables").empty());
    CHECK(visible<QWidget>(*list, "AddressStatus").empty());

    auto& c = cable(*doc, out, in);
    settle();
    {
      auto rows = visible<QLabel>(*list, "PortCable");
      REQUIRE(rows.size() == 1);
      CHECK(rows.front()->text().contains("Source"));
      CHECK(rows.front()->text().contains(out.name()));
      // The inlet now has an address that is only read while the source is idle
      auto ind = visible<QWidget>(*list, "AddressStatus");
      REQUIRE(ind.size() == 1);
      CHECK(ind.front()->property("addressStatus").toInt()
            == Process::AddressStatus::WhileCablesIdle);
      CHECK(!ind.front()->toolTip().isEmpty());
    }

    // The row follows a rename of the other end and a change of cable type
    a->metadata().setName("Renamed");
    c.setType(Process::CableType::DelayedStrict);
    settle();
    {
      auto rows = visible<QLabel>(*list, "PortCable");
      REQUIRE(rows.size() == 1);
      CHECK(rows.front()->text().contains("Renamed"));
      CHECK(rows.front()->text().contains("delayed"));
      auto ind = visible<QWidget>(*list, "AddressStatus");
      REQUIRE(ind.size() == 1);
      CHECK(ind.front()->property("addressStatus").toInt()
            == Process::AddressStatus::Unused);
    }

    // The cable button selects the cable, the link the process at the other end
    {
      auto btn = visible<QToolButton>(*list, "SelectCable");
      REQUIRE(btn.size() == 1);
      CHECK(!btn.front()->icon().isNull());
      CHECK(btn.front()->text().isEmpty());
      CHECK(!visible<QLabel>(*list, "PortCable").front()->text().startsWith("From"));
      btn.front()->click();
      settle();
      CHECK(dc.selectionStack.currentSelection().contains(&c));
    }
    // The selection may have rebuilt nothing here: the list is not in an
    // inspector, so it is still there to click
    {
      auto rows = visible<QLabel>(*list, "PortCable");
      REQUIRE(rows.size() == 1);
      Q_EMIT rows.front()->linkActivated("owner");
      settle();
      CHECK(dc.selectionStack.currentSelection().contains(a));
    }

    // Undoing the cable removes the row and the indicator
    doc->commandStack().undo();
    settle();
    CHECK(visible<QWidget>(*list, "PortCables").empty());
    CHECK(visible<QWidget>(*list, "AddressStatus").empty());
  });
}

TEST_CASE("the audio output inspector edits through commands", "[integration][inspector]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    if(!a)
      SKIP("the media plug-in is not built");
    auto& out = *qobject_cast<Process::AudioOutlet*>(a->outlets().front());

    QWidget window;
    auto list = new Process::PortListWidget{*a, dc, &window};
    window.show();
    settle();

    auto checks = list->findChildren<QCheckBox*>("Propagate");
    REQUIRE(checks.size() == 1);
    auto cb = checks.front();
    const bool before = out.propagate();
    CHECK(cb->isChecked() == before);
    const int n = doc->commandStack().currentIndex();
    cb->click();
    CHECK(out.propagate() == !before);
    CHECK(doc->commandStack().currentIndex() == n + 1);
    doc->commandStack().undo();
    CHECK(out.propagate() == before);
    CHECK(cb->isChecked() == before);

    // The gain is shown next to Propagate
    auto gains = list->findChildren<score::DoubleSlider*>("Gain");
    REQUIRE(gains.size() == 1);
    auto gain = gains.front();
    CHECK(gain->isVisibleTo(list));
    CHECK(gain->value() == out.gain());

    // A change made elsewhere is shown without a command
    const int m = doc->commandStack().currentIndex();
    CommandDispatcher<>{dc.commandStack}.submit<Process::SetGain>(out, 0.5);
    CHECK(doc->commandStack().currentIndex() == m + 1);
    CHECK(gain->value() == 0.5);

    // Moving the slider is one command, merged until released
    Q_EMIT gain->sliderMoved(0.25);
    Q_EMIT gain->sliderMoved(0.3);
    Q_EMIT gain->sliderReleased();
    CHECK(out.gain() == 0.3);
    CHECK(doc->commandStack().currentIndex() == m + 2);
    doc->commandStack().undo();
    CHECK(out.gain() == 0.5);

    // A value typed in the pop-up is one undo step, committed when it closes
    gain->createPopup(QPoint{0, 0});
    QDoubleSpinBox* spin{};
    for(auto w : QApplication::topLevelWidgets())
      if(auto sb = qobject_cast<QDoubleSpinBox*>(w); sb && sb->isVisible())
        spin = sb;
    REQUIRE(spin);
    spin->setValue(0.6);
    spin->setValue(0.75);
    // Enter, then the focus loss of the closing pop-up
    Q_EMIT spin->editingFinished();
    Q_EMIT spin->editingFinished();
    CHECK(out.gain() == 0.75);
    CHECK(doc->commandStack().currentIndex() == m + 2);
    doc->commandStack().undo();
    CHECK(out.gain() == 0.5);
    CHECK(gain->value() == 0.5);
    settle();
  });
}

TEST_CASE(
    "the audio output panel sits above its port in a zoomed out nodal view",
    "[integration][inspector][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    if(!a)
      SKIP("the media plug-in is not built");
    auto& out = *qobject_cast<Process::AudioOutlet*>(a->outlets().front());

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    // The nodal view zooms by scaling the container of its nodes
    QGraphicsScene scene;
    auto container = new QGraphicsRectItem{};
    scene.addItem(container);
    auto item = new Dataflow::AudioOutletItem{out, pctx, container};
    item->setPos(100, 200);

    for(double zoom : {0.5, 0.25, 1., 1.5})
    {
      container->setScale(zoom);
      QGraphicsSceneMouseEvent press{QEvent::GraphicsSceneMousePress};
      press.setButton(Qt::LeftButton);
      press.setButtons(Qt::LeftButton);
      press.setPos({5, 5});
      press.setScenePos(item->mapToScene(5, 5));
      item->mousePressEvent(&press);
      QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
      release.setButton(Qt::LeftButton);
      release.setPos({5, 5});
      release.setScenePos(item->mapToScene(5, 5));
      item->mouseReleaseEvent(&release);
      settle();

      INFO("zoom " << zoom);
      REQUIRE(item->m_subView);
      const QRectF p = item->m_subView->sceneBoundingRect();
      const QRectF port = item->sceneBoundingRect();
      CHECK(p.bottom() < port.top());
      CHECK(port.top() - p.bottom() < 10.);
      CHECK(std::abs(p.left() - item->scenePos().x()) < 1e-6);

      // A second click closes it
      item->mousePressEvent(&press);
      item->mouseReleaseEvent(&release);
      settle();
      CHECK(!item->m_subView);
    }

    delete item;
    delete container;
  });
}

TEST_CASE("the audio output panel of the nodal view", "[integration][inspector][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    if(!a)
      SKIP("the media plug-in is not built");
    auto& out = *qobject_cast<Process::AudioOutlet*>(a->outlets().front());

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    QGraphicsScene scene;
    auto item = new Dataflow::AudioOutletItem{out, pctx, nullptr};
    scene.addItem(item);
    item->setPos(100, 200);

    QGraphicsSceneMouseEvent press{QEvent::GraphicsSceneMousePress};
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    press.setPos({5, 5});
    press.setScenePos(item->mapToScene(5, 5));
    item->mousePressEvent(&press);
    QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
    release.setButton(Qt::LeftButton);
    release.setPos({5, 5});
    release.setScenePos(item->mapToScene(5, 5));
    item->mouseReleaseEvent(&release);
    settle();

    REQUIRE(item->m_subView);
    auto panel = item->m_subView.data();
    // Above the port, pointing at it
    CHECK(panel->sceneBoundingRect().bottom() < item->sceneBoundingRect().top());

    score::QGraphicsCheckBox* propagate{};
    score::QGraphicsSlider* gain{};
    for(auto child : panel->childItems())
    {
      if(auto c = qgraphicsitem_cast<score::QGraphicsCheckBox*>(child))
        propagate = c;
      if(auto s = qgraphicsitem_cast<score::QGraphicsSlider*>(child))
        gain = s;
    }
    REQUIRE(propagate);
    REQUIRE(gain);
    // The view closes the panel on a click on an item whose origin is outside it
    for(auto child : panel->childItems())
      CHECK(panel->contains(child->pos()));
    CHECK(gain->value() == out.gain());

    // Propagate: one command, undoable, and the box follows the undo
    const bool before = out.propagate();
    const int n = doc->commandStack().currentIndex();
    Q_EMIT propagate->toggled(!before);
    CHECK(out.propagate() == !before);
    CHECK(doc->commandStack().currentIndex() == n + 1);
    doc->commandStack().undo();
    CHECK(out.propagate() == before);

    // Gain: an ongoing command committed on release, one undo step
    const double gainBefore = out.gain();
    gain->setValue(0.3);
    Q_EMIT gain->sliderMoved();
    gain->setValue(0.4);
    Q_EMIT gain->sliderMoved();
    Q_EMIT gain->sliderReleased();
    CHECK(out.gain() == 0.4);
    // The undone propagation is replaced by it
    CHECK(doc->commandStack().currentIndex() == n + 1);
    doc->commandStack().undo();
    CHECK(out.gain() == gainBefore);
    CHECK(gain->value() == gainBefore);
    doc->commandStack().redo();
    CHECK(out.gain() == 0.4);
    CHECK(gain->value() == 0.4);
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 0.7);
    CHECK(gain->value() == 0.7);

    // The panel grows with the cables and keeps pointing at the port
    auto* b = score::test::add_process(*doc, mergerUuid, {});
    REQUIRE(b);
    const double h = panel->boundingRect().height();
    cable(*doc, out, *b->inlets().front());
    settle();
    CHECK(panel->boundingRect().height() > h);
    CHECK(panel->sceneBoundingRect().bottom() < item->sceneBoundingRect().top());

    delete item;
  });
}

TEST_CASE("texture input settings are in a drawer", "[integration][inspector][gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = score::test::add_process(*doc, isfUuid, {});
    if(!p)
      SKIP("the gfx plug-in is not built");
    REQUIRE(!p->inlets().empty());
    REQUIRE(p->inlets().front()->type() == Process::PortType::Texture);

    QWidget window;
    auto list = new Process::PortListWidget{*p, doc->context(), &window};
    window.show();
    settle();

    auto drawers = list->findChildren<QWidget*>("PortDrawer");
    QWidget* drawer{};
    for(auto d : drawers)
      if(d->findChild<QComboBox*>() && d->findChildren<QComboBox*>().size() >= 4)
        drawer = d;
    REQUIRE(drawer);

    // Everything is left to the shader: the drawer starts closed
    CHECK(!drawer->isVisibleTo(list));

    // The arrow in the row of the port opens it
    score::ArrowButton* button{};
    for(auto b : list->findChildren<score::ArrowButton*>("PortDrawerButton"))
      if(b->isVisibleTo(list) && !button)
        button = b;
    REQUIRE(button);
    button->click();
    CHECK(drawer->isVisibleTo(list));
    button->click();
    CHECK(!drawer->isVisibleTo(list));
  });
}

TEST_CASE("a control lists its cables in its drawer", "[integration][inspector]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, lfoUuid, {});
    auto* b = score::test::add_process(*doc, lfoUuid, {});
    if(!a || !b)
      SKIP("the fx plug-in is not built");
    a->metadata().setName("Source");

    Process::Port* out{};
    for(auto o : a->outlets())
      if(o->type() == Process::PortType::Message && !out)
        out = o;
    Process::Port* in{};
    for(auto i : b->inlets())
      if(qobject_cast<Process::ControlInlet*>(i) && !in)
        in = i;
    REQUIRE(out);
    REQUIRE(in);
    cable(*doc, *out, *in);

    QWidget window;
    auto list = new Process::PortListWidget{*b, doc->context(), &window};
    window.show();
    settle();

    // The control's cable is in its drawer, closed with it
    QWidget* drawer{};
    for(auto d : list->findChildren<QWidget*>("PortDrawer"))
      if(auto c = d->findChild<QWidget*>("PortCables"); c && c->isVisibleTo(d))
        drawer = d;
    REQUIRE(drawer);
    CHECK(!drawer->isVisibleTo(list));
    CHECK(visible<QLabel>(*list, "PortCable").empty());

    // Opening the drawer of that control shows the cable
    for(auto btn : list->findChildren<score::ArrowButton*>("PortDrawerButton"))
    {
      if(!btn->isVisibleTo(list))
        continue;
      btn->click();
      if(drawer->isVisibleTo(list))
        break;
      btn->click();
    }
    REQUIRE(drawer->isVisibleTo(list));
    settle();
    auto rows = visible<QLabel>(*list, "PortCable");
    REQUIRE(rows.size() == 1);
    CHECK(rows.front()->text().contains("Source"));
  });
}

TEST_CASE("a texture input lists its cables in its drawer", "[integration][inspector][gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, isfUuid, {});
    auto* b = score::test::add_process(*doc, isfUuid, {});
    if(!a || !b)
      SKIP("the gfx plug-in is not built");
    a->metadata().setName("Source");

    Process::Port* out{};
    for(auto o : a->outlets())
      if(o->type() == Process::PortType::Texture && !out)
        out = o;
    auto* in = b->inlets().front();
    REQUIRE(out);
    REQUIRE(in->type() == Process::PortType::Texture);
    cable(*doc, *out, *in);

    QWidget window;
    auto list = new Process::PortListWidget{*b, doc->context(), &window};
    window.show();
    settle();

    // The cable is in the drawer of the texture input, after its settings
    QWidget* drawer{};
    for(auto d : list->findChildren<QWidget*>("PortDrawer"))
      if(auto c = d->findChild<QWidget*>("PortCables"); c && c->isVisibleTo(d)
         && d->findChildren<QComboBox*>().size() >= 4)
        drawer = d;
    REQUIRE(drawer);
    CHECK(!drawer->isVisibleTo(list));
    CHECK(visible<QLabel>(*list, "PortCable").empty());
    // Not listed a second time outside of the drawer
    for(auto c : list->findChildren<QWidget*>("PortCables"))
      if(c->isVisibleTo(list))
        CHECK(drawer->isAncestorOf(c));

    for(auto btn : list->findChildren<score::ArrowButton*>("PortDrawerButton"))
    {
      if(!btn->isVisibleTo(list))
        continue;
      btn->click();
      if(drawer->isVisibleTo(list))
        break;
      btn->click();
    }
    REQUIRE(drawer->isVisibleTo(list));
    settle();
    auto rows = visible<QLabel>(*list, "PortCable");
    REQUIRE(rows.size() == 1);
    CHECK(rows.front()->text().contains("Source"));
    CHECK(drawer->isAncestorOf(rows.front()));
  });
}

// Some older documents saved texture and geometry ports with no name at all:
// the inspector names them as their node does.
TEST_CASE("a port saved without a name is labelled as its node shows it", "[integration][inspector][gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = score::test::add_process(*doc, isfUuid, {});
    if(!p)
      SKIP("the gfx plug-in is not built");
    auto& tex = *p->inlets().front();
    REQUIRE(tex.type() == Process::PortType::Texture);
    const auto procId = p->id();

    // The same document, with the name of that port taken out of the file
    QByteArray json = score::test::save_as_json(*doc);
    for(const QString& key : {QStringLiteral("Custom"), QStringLiteral("Exposed")})
    {
      const QString value = key == QStringLiteral("Custom") ? tex.name() : tex.exposed();
      const QByteArray field
          = QStringLiteral("\"%1\":\"%2\",").arg(key, value).toUtf8();
      REQUIRE(json.contains(field));
      json.replace(field, QByteArray{});
    }
    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* reloaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("unnamed"), json, JSONObject::type(), *delegates.begin());
    REQUIRE(reloaded);
    settle();

    Process::ProcessModel* proc{};
    for(auto* candidate : reloaded->findChildren<Process::ProcessModel*>())
      if(candidate->id() == procId && candidate->concreteKey() == p->concreteKey())
        proc = candidate;
    REQUIRE(proc);
    REQUIRE(proc->inlets().front()->visualName().isEmpty());
    CHECK(Process::displayName(*proc->inlets().front()) == QStringLiteral("Texture In"));

    QWidget window;
    auto list = new Process::PortListWidget{*proc, reloaded->context(), &window};
    window.show();
    settle();

    bool labelled = false;
    for(auto* l : list->findChildren<QLabel*>())
      labelled |= l->text() == QStringLiteral("Texture In") && l->isVisibleTo(list);
    CHECK(labelled);
  });
}

namespace
{
//! Fills every channel of an audio outlet with ones, as a node would before
//! the graph post-processes the outlet.
void fillOnes(ossia::audio_outlet& port)
{
  auto& data = port.data;
  data.set_channels(2);
  for(std::size_t c = 0; c < data.channels(); c++)
    data.channel(c).assign(16, 1.);
}

bool allEqual(const ossia::audio_outlet& port, double v)
{
  for(std::size_t c = 0; c < port.data.channels(); c++)
    for(double s : port.data.channel(c))
      if(s != v)
        return false;
  return port.data.channels() > 0;
}

//! Runs the pending execution commands and says how many there were
int drain(Execution::DocumentPlugin& plug)
{
  int n = 0;
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  Execution::ExecutionCommand cmd;
  while(plug.context().executionQueue.try_dequeue(cmd))
  {
    cmd();
    n++;
  }
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  return n;
}
}

TEST_CASE("the gain of a process output is applied by the engine", "[integration][exec]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* a = score::test::add_process(*doc, mergerUuid, {});
    if(!a)
      SKIP("the media plug-in is not built");
    auto& out = *qobject_cast<Process::AudioOutlet*>(a->outlets().front());
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 0.25);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(a->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto root = it->second->node->root_outputs().front();
    REQUIRE(root->which() == ossia::audio_port::which);
    auto& exec = *static_cast<ossia::audio_outlet*>(root);
    CHECK(exec.gain == 0.25);

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    fillOnes(exec);
    exec.post_process();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    CHECK(allEqual(exec, 0.25));

    // Follows the model while playing, undo included
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 0.5);
    score::test::run_exec(plug);
    CHECK(exec.gain == 0.5);
    fillOnes(exec);
    exec.post_process();
    CHECK(allEqual(exec, 0.5));
    doc->commandStack().undo();
    score::test::run_exec(plug);
    CHECK(exec.gain == 0.25);

    // A value reaching the Gain port, from a cable or an automation, sets the
    // gain while it plays without editing the model
    auto& setup = plug.context().setup;
    auto gainIn = setup.inlets.find(out.gainInlet.get());
    REQUIRE(gainIn != setup.inlets.end());
    CHECK(gainIn->second.second == &exec.gain_inlet);
    exec.gain_inlet.data.write_value(0.125f, 0);
    fillOnes(exec);
    exec.post_process();
    CHECK(allEqual(exec, 0.125));
    CHECK(out.gain() == 0.25);
    exec.gain_inlet.data.clear();

    // The next edit of the trim takes over again
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 1.);
    score::test::run_exec(plug);
    fillOnes(exec);
    exec.post_process();
    CHECK(allEqual(exec, 1.));
  });
}

TEST_CASE("the gain of an interval output is applied by the engine", "[integration][exec]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    REQUIRE(itv.outlet);
    auto& out = *itv.outlet;
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 0.25);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, itv);
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& ossia_itv = plug.baseScenario()->baseInterval().OSSIAInterval();
    REQUIRE(ossia_itv);
    auto& exec = static_cast<ossia::nodes::interval*>(ossia_itv->node.get())->audio_out;
    CHECK(exec.gain == 0.25);

    // One registration follows the model: a single command per change
    drain(plug);
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetGain>(out, 0.5);
    CHECK(drain(plug) == 1);
    CHECK(exec.gain == 0.5);
    doc->commandStack().undo();
    score::test::run_exec(plug);
    CHECK(exec.gain == 0.25);
  });
}

