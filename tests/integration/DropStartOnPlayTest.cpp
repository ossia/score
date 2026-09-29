// Alt while dropping in a scenario drops without magnetism and gives the new
// time sync a trigger with start-on-play, exactly like a double-click in the
// scenario does. It holds both for drops that make an interval and for drops
// that make a state. What always lands in a box in the void (a port, a preset
// from the library or a file, the copy of a preset button) gets it without Alt.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <State/MessageListSerialization.hpp>

#include <Effect/EffectLayer.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Dataflow/PortItem.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Application/Drops/PresetDrop.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/graphics/widgets/QGraphicsSelectablePixmapToggle.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <qpa/qwindowsysteminterface.h>

#include <catch2/catch_test_macros.hpp>

#include <ossia/detail/algorithms.hpp>

#include <vector>

namespace
{
// The drop handlers read qApp->keyboardModifiers(), which only the platform
// layer writes: a synthesized QKeyEvent would not reach it.
void holdModifier(Qt::KeyboardModifier mod, int key, bool held)
{
  const auto mods = held ? mod : Qt::NoModifier;
  QWindowSystemInterface::handleKeyEvent(
      nullptr, held ? QEvent::KeyPress : QEvent::KeyRelease, key, mods);
  QWindowSystemInterface::flushWindowSystemEvents();
  REQUIRE(QGuiApplication::keyboardModifiers() == mods);
}

void holdAlt(bool held)
{
  holdModifier(Qt::AltModifier, Qt::Key_Alt, held);
}

void holdCtrl(bool held)
{
  holdModifier(Qt::ControlModifier, Qt::Key_Control, held);
}

std::vector<Id<Scenario::IntervalModel>> intervalIds(const Scenario::ProcessModel& sc)
{
  std::vector<Id<Scenario::IntervalModel>> res;
  for(auto& itv : sc.intervals)
    res.push_back(itv.id());
  return res;
}

std::vector<Id<Scenario::StateModel>> stateIds(const Scenario::ProcessModel& sc)
{
  std::vector<Id<Scenario::StateModel>> res;
  for(auto& st : sc.states)
    res.push_back(st.id());
  return res;
}

std::vector<const Scenario::TimeSyncModel*>
triggers(const Scenario::ProcessModel& scenario)
{
  std::vector<const Scenario::TimeSyncModel*> res;
  for(auto& ts : scenario.timeSyncs)
    if(ts.active())
      res.push_back(&ts);
  return res;
}

void doubleClick(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  auto& view = pres.view();
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseDoubleClick};
  ev.setPos(pos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  view.scene()->sendEvent(&view, &ev);
  QApplication::processEvents();
}

void sendDrop(Scenario::ScenarioPresenter& pres, QPointF pos, QMimeData& mime)
{
  auto& view = pres.view();
  QGraphicsSceneDragDropEvent ev{QEvent::GraphicsSceneDrop};
  ev.setPos(pos);
  ev.setMimeData(&mime);
  ev.setModifiers(QGuiApplication::keyboardModifiers());
  view.scene()->sendEvent(&view, &ev);
  QApplication::processEvents();
}

void dropProcess(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  QMimeData mime;
  Mime<Process::ProcessData>::Serializer s{mime};
  s.serialize(Process::ProcessData{
      Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), QStringLiteral("Scenario"),
      {}});

  sendDrop(pres, pos, mime);
}

void dropMessages(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  State::Message msg;
  msg.address = State::AddressAccessor{State::Address{"dev", {"foo"}}};
  msg.value = 1.f;

  QMimeData mime;
  Mime<State::MessageList>::Serializer s{mime};
  s.serialize(State::MessageList{msg});

  sendDrop(pres, pos, mime);
}
}

TEST_CASE(
    "Alt while dropping a process makes it start on play",
    "[integration][scenario][drop][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                     .focusedPresenter();
    REQUIRE(pres);
    REQUIRE(&pres->model() == &scenario);

    auto& stack = doc->commandStack();
    const auto intervals_before = scenario.intervals.size();
    const QPointF pos{300., 60.};
    REQUIRE(triggers(scenario).empty());

    // A plain drop creates the interval and leaves every sync untriggered.
    holdAlt(false);
    dropProcess(*pres, pos);

    CHECK(scenario.intervals.size() > intervals_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.intervals.size() == intervals_before);

    // Ctrl is not a modifier the drop reads.
    holdCtrl(true);
    dropProcess(*pres, pos);
    holdCtrl(false);

    CHECK(scenario.intervals.size() > intervals_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.intervals.size() == intervals_before);

    // A double-click: the reference behaviour.
    doubleClick(*pres, pos);

    const auto clicked = triggers(scenario);
    REQUIRE(clicked.size() == 1);
    CHECK(clicked.front()->active());
    CHECK(clicked.front()->isStartPoint());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(triggers(scenario).empty());

    // The same, obtained by holding alt during the drop.
    const auto before = intervalIds(scenario);
    holdAlt(true);
    dropProcess(*pres, pos);
    holdAlt(false);

    REQUIRE(scenario.intervals.size() == intervals_before + 1);
    const Scenario::IntervalModel* itv{};
    for(auto& i : scenario.intervals)
      if(!ossia::contains(before, i.id()))
        itv = &i;
    REQUIRE(itv);
    auto& sync = Scenario::startTimeSync(*itv, scenario);

    CHECK(sync.active());
    CHECK(sync.isStartPoint());

    const auto dropped_trig = triggers(scenario);
    REQUIRE(dropped_trig.size() == 1);
    CHECK(dropped_trig.front() == &sync);

    // One undo takes the whole drop away, trigger included.
    stack.undo();
    QApplication::processEvents();
    CHECK(scenario.intervals.size() == intervals_before);
    CHECK(triggers(scenario).empty());
  });
}

TEST_CASE(
    "Alt while dropping messages makes the state start on play",
    "[integration][scenario][drop][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                     .focusedPresenter();
    REQUIRE(pres);

    auto& stack = doc->commandStack();
    const auto states_before = scenario.states.size();
    const QPointF pos{300., 60.};
    REQUIRE(triggers(scenario).empty());

    // A plain drop creates the state and leaves every sync untriggered.
    holdAlt(false);
    dropMessages(*pres, pos);

    CHECK(scenario.states.size() > states_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.states.size() == states_before);

    const auto before = stateIds(scenario);
    holdAlt(true);
    dropMessages(*pres, pos);
    holdAlt(false);

    REQUIRE(scenario.states.size() == states_before + 1);
    const Scenario::StateModel* st{};
    for(auto& s : scenario.states)
      if(!ossia::contains(before, s.id()))
        st = &s;
    REQUIRE(st);
    auto& sync = Scenario::parentTimeSync(*st, scenario);

    CHECK(sync.active());
    CHECK(sync.isStartPoint());

    const auto dropped_trig = triggers(scenario);
    REQUIRE(dropped_trig.size() == 1);
    CHECK(dropped_trig.front() == &sync);

    stack.undo();
    QApplication::processEvents();
    CHECK(scenario.states.size() == states_before);
    CHECK(triggers(scenario).empty());
  });
}

// A port dropped in the scenario makes an automation with the start-on-play
// trigger a double-click gives, not a "flying" one after nothing that would
// start it.
TEST_CASE(
    "A port dropped in the scenario makes an automation that starts on play",
    "[integration][scenario][drop][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());
    auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                     .focusedPresenter();
    REQUIRE(pres);

    // Something with a control to automate: the LFO
    Process::ProcessModel* lfo{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
      lfo = m.createProcessInNewSlot(
          root, UuidKey<Process::ProcessModel>{"0b1b1816-c33e-4796-a16d-5aab27fe600f"},
          {}, QPointF{});
      m.commit();
    }
    if(!lfo)
      SKIP("LFO not built");
    Process::ControlInlet* ctl{};
    for(auto* in : lfo->inlets())
      if(auto c = qobject_cast<Process::ControlInlet*>(in);
         c && c->type() == Process::PortType::Message)
      {
        ctl = c;
        break;
      }
    REQUIRE(ctl);

    Dataflow::AutomatablePortItem item{*ctl, pres->context().context, nullptr};
    Dataflow::PortItem::clickedPort = &item;

    auto& stack = doc->commandStack();
    const auto before = intervalIds(scenario);
    REQUIRE(triggers(scenario).empty());

    holdAlt(false);
    QMimeData mime;
    mime.setData(score::mime::port(), {});
    sendDrop(*pres, {300., 60.}, mime);
    Dataflow::PortItem::clickedPort = nullptr;

    REQUIRE(scenario.intervals.size() == before.size() + 1);
    const Scenario::IntervalModel* itv{};
    for(auto& i : scenario.intervals)
      if(!ossia::contains(before, i.id()))
        itv = &i;
    REQUIRE(itv);
    CHECK(!itv->processes.empty());
    auto& sync = Scenario::startTimeSync(*itv, scenario);
    CHECK(sync.active());
    CHECK(sync.isStartPoint());

    stack.undo();
    QApplication::processEvents();
    CHECK(scenario.intervals.size() == before.size());
    CHECK(triggers(scenario).empty());
  });
}

namespace
{
struct PresetFixture
{
  score::Document* doc{};
  Scenario::IntervalModel* root{};
  Scenario::ProcessModel* scenario{};
  Scenario::ScenarioPresenter* pres{};
  Process::ProcessModel* source{};
};

// A process with controls, in the root interval: the source of the presets.
PresetFixture makePresetFixture(const score::GUIApplicationContext& ctx)
{
  PresetFixture f;
  f.doc = score::test::new_document(ctx);
  REQUIRE(f.doc);
  f.root = &static_cast<Scenario::ScenarioDocumentModel&>(
                f.doc->model().modelDelegate())
                .baseInterval();
  f.scenario = &static_cast<Scenario::ProcessModel&>(*f.root->processes.begin());
  f.pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
               .focusedPresenter();
  REQUIRE(f.pres);
  REQUIRE(&f.pres->model() == f.scenario);

  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, f.doc->context()};
  f.source = m.createProcessInNewSlot(
      *f.root, UuidKey<Process::ProcessModel>{"0b1b1816-c33e-4796-a16d-5aab27fe600f"},
      {}, QPointF{});
  m.commit();
  QApplication::processEvents();
  return f;
}

QByteArray presetJson(const Process::ProcessModel& proc, const QString& name)
{
  auto preset = proc.savePreset();
  preset.name = name;
  return preset.toJson();
}

const Scenario::IntervalModel*
newInterval(const Scenario::ProcessModel& sc, const std::vector<Id<Scenario::IntervalModel>>& before)
{
  const Scenario::IntervalModel* res{};
  for(auto& i : sc.intervals)
    if(!ossia::contains(before, i.id()))
    {
      REQUIRE(!res);
      res = &i;
    }
  return res;
}

// The dropped preset is in its own new box, whose start has the trigger of a
// double-click; a single undo takes all of it back and redo restores it.
void checkPresetBoxStartsOnPlay(
    PresetFixture& f, const std::vector<Id<Scenario::IntervalModel>>& before,
    const QString& name)
{
  auto& sc = *f.scenario;
  auto& stack = f.doc->commandStack();

  auto check = [&] {
    REQUIRE(sc.intervals.size() == before.size() + 1);
    auto itv = newInterval(sc, before);
    REQUIRE(itv);
    REQUIRE(itv->processes.size() == 1);
    CHECK(itv->processes.begin()->concreteKey() == f.source->concreteKey());
    CHECK(itv->processes.begin()->metadata().getName() == name);
    auto& sync = Scenario::startTimeSync(*itv, sc);
    CHECK(sync.active());
    CHECK(sync.isStartPoint());
    const auto trig = triggers(sc);
    REQUIRE(trig.size() == 1);
    CHECK(trig.front() == &sync);
  };
  check();

  stack.undo();
  QApplication::processEvents();
  CHECK(sc.intervals.size() == before.size());
  CHECK(triggers(sc).empty());

  stack.redo();
  QApplication::processEvents();
  check();

  stack.undo();
  QApplication::processEvents();
  REQUIRE(triggers(sc).empty());
}
}

// A library preset always lands in a box of its own in the void, after nothing
// that would start it: like a double-click's box, it starts on play, whatever
// the modifiers.
TEST_CASE(
    "A library preset dropped in the scenario starts on play",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto f = makePresetFixture(ctx);
    if(!f.source)
      SKIP("LFO not built");
    REQUIRE(triggers(*f.scenario).empty());

    for(auto [mod, key] :
        {std::pair{Qt::NoModifier, 0}, std::pair{Qt::AltModifier, int(Qt::Key_Alt)},
         std::pair{Qt::ControlModifier, int(Qt::Key_Control)},
         std::pair{Qt::ShiftModifier, int(Qt::Key_Shift)}})
    {
      INFO("modifier " << int(mod));
      const auto before = intervalIds(*f.scenario);
      if(key)
        holdModifier(mod, key, true);
      QMimeData mime;
      mime.setData(score::mime::processpreset(), presetJson(*f.source, "My LFO"));
      sendDrop(*f.pres, {300., 60.}, mime);
      if(key)
        holdModifier(mod, key, false);

      checkPresetBoxStartsOnPlay(f, before, "My LFO");
    }
  });
}

TEST_CASE(
    "A preset file dropped in the scenario starts on play",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto f = makePresetFixture(ctx);
    if(!f.source)
      SKIP("LFO not built");

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("Slow LFO.scp");
    {
      QFile file{path};
      REQUIRE(file.open(QIODevice::WriteOnly));
      file.write(presetJson(*f.source, "Slow LFO"));
    }

    const auto before = intervalIds(*f.scenario);
    holdAlt(false);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    sendDrop(*f.pres, {300., 60.}, mime);

    checkPresetBoxStartsOnPlay(f, before, "Slow LFO");
  });
}

// The preset button of a process, dropped in the scenario as a copy (its
// palette's "Copy in a new box"): the copy is in a box in the void too.
TEST_CASE(
    "The preset button's copy dropped in the scenario starts on play",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto f = makePresetFixture(ctx);
    if(!f.source)
      SKIP("LFO not built");
    f.source->metadata().setName("Source LFO");

    std::unique_ptr<score::QGraphicsDraggablePixmap> button{
        Process::makePresetButton(*f.source, f.doc->context(), nullptr, nullptr)};
    REQUIRE(button->createDrag);

    // The choice Ctrl's palette offers, without the menu
    struct Chooser
    {
      Chooser()
      {
        Scenario::presetDropChooser = [](const Scenario::PresetDropChoices& c)
            -> std::optional<Scenario::PresetDrop> {
          REQUIRE(c.copy);
          return Scenario::PresetDrop::Copy;
        };
      }
      ~Chooser() { Scenario::presetDropChooser = {}; }
    } chooser;

    for(bool alt : {false, true})
    {
      INFO("alt " << alt);
      const auto before = intervalIds(*f.scenario);
      holdAlt(alt);
      QMimeData mime;
      button->createDrag(mime);
      sendDrop(*f.pres, {300., 60.}, mime);
      holdAlt(false);

      checkPresetBoxStartsOnPlay(f, before, "Source LFO");
    }
  });
}

// A library preset dropped on a state goes in a new box after that state, as a
// process dropped there does; that box starts with the state, it gets no
// trigger of its own.
TEST_CASE(
    "A library preset dropped on a state makes a box after it",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto f = makePresetFixture(ctx);
    if(!f.source)
      SKIP("LFO not built");
    auto& sc = *f.scenario;
    auto& stack = f.doc->commandStack();

    // A state of its own, without a following interval
    Id<Scenario::StateModel> state_id;
    {
      Scenario::Command::Macro m{
          new Scenario::Command::CreateStateMacro, f.doc->context()};
      const auto& [t, e, s] = m.createDot(sc, {TimeVal::fromMsecs(2000), 0.3});
      state_id = s.id();
      m.commit();
    }
    QApplication::processEvents();
    auto& state = sc.state(state_id);
    REQUIRE(!state.nextInterval());
    auto* st_view = f.pres->state(state_id).view();
    REQUIRE(st_view);

    const auto before = intervalIds(sc);
    const auto triggers_before = triggers(sc);

    auto drop = [&] {
      QMimeData mime;
      mime.setData(score::mime::processpreset(), presetJson(*f.source, "State LFO"));
      QGraphicsSceneDragDropEvent ev{QEvent::GraphicsSceneDrop};
      ev.setPos({});
      ev.setMimeData(&mime);
      ev.setModifiers(QGuiApplication::keyboardModifiers());
      st_view->scene()->sendEvent(st_view, &ev);
      QApplication::processEvents();
    };

    auto check = [&] {
      REQUIRE(sc.intervals.size() == before.size() + 1);
      auto itv = newInterval(sc, before);
      REQUIRE(itv);
      CHECK(itv->startState() == state_id);
      REQUIRE(itv->processes.size() == 1);
      CHECK(itv->processes.begin()->concreteKey() == f.source->concreteKey());
      CHECK(itv->processes.begin()->metadata().getName() == "State LFO");
      CHECK(triggers(sc) == triggers_before);
    };

    holdAlt(false);
    drop();
    check();

    stack.undo();
    QApplication::processEvents();
    CHECK(sc.intervals.size() == before.size());
    CHECK(!state.nextInterval());

    stack.redo();
    QApplication::processEvents();
    check();
  });
}

// On an interval (its header, or its nodal view: both go through the interval
// drop handlers) the preset goes into the interval, which already exists and
// keeps its syncs untouched.
TEST_CASE(
    "A library preset dropped on an interval loads into it",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto f = makePresetFixture(ctx);
    if(!f.source)
      SKIP("LFO not built");
    auto& sc = *f.scenario;
    auto& stack = f.doc->commandStack();

    const Scenario::IntervalModel* itv{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, f.doc->context()};
      itv = &m.createBox(sc, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(4000), 0.4);
      m.commit();
    }
    QApplication::processEvents();
    REQUIRE(itv->processes.empty());
    const auto intervals_before = sc.intervals.size();

    QMimeData mime;
    mime.setData(score::mime::processpreset(), presetJson(*f.source, "Itv LFO"));
    REQUIRE(ctx.interfaces<Scenario::IntervalDropHandlerList>().drop(
        f.doc->context(), *itv, {}, mime));
    QApplication::processEvents();

    auto check = [&] {
      CHECK(sc.intervals.size() == intervals_before);
      REQUIRE(itv->processes.size() == 1);
      CHECK(itv->processes.begin()->metadata().getName() == "Itv LFO");
      CHECK(triggers(sc).empty());
    };
    check();
    stack.undo();
    QApplication::processEvents();
    CHECK(itv->processes.empty());
    stack.redo();
    QApplication::processEvents();
    check();
  });
}
