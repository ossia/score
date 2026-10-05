// Entering and leaving a sub-interval in the nodal view keeps the play bar on
// the interval view currently shown. A paste goes in the last focused or
// selected object if it can take what was copied, else in its first parent
// that can.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QMimeData>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
const UuidKey<Process::ProcessModel> automationKey
    = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"));

TimeVal seconds(double s)
{
  return TimeVal::fromMsecs(s * 1000.);
}

Scenario::ProcessModel& scenarioIn(Scenario::IntervalModel& itv)
{
  for(auto& p : itv.processes)
    if(auto sc = qobject_cast<Scenario::ProcessModel*>(&p))
      return *sc;
  FAIL("no scenario in the interval");
  std::abort();
}

//! The root scenario holds
//!   b1, with an automation a1,
//!   b2, with a scenario `sub`, which holds b3, with an automation a3.
struct Nested
{
  score::Document& doc;
  Scenario::ScenarioDocumentPresenter& pres;
  Scenario::IntervalModel& base;
  Scenario::ProcessModel& top;
  Scenario::IntervalModel* b1{};
  Process::ProcessModel* a1{};
  Scenario::IntervalModel* b2{};
  Scenario::ProcessModel* sub{};
  Scenario::IntervalModel* b3{};
  Process::ProcessModel* a3{};

  explicit Nested(score::Document& d)
      : doc{d}
      , pres{*score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(d)}
      , base{score::IDocument::get<Scenario::ScenarioDocumentModel>(d).baseInterval()}
      , top{scenarioIn(base)}
  {
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
    b1 = &m.createBox(top, seconds(0.5), seconds(3.), 0.05);
    a1 = m.createProcess(*b1, automationKey, {}, {});
    b2 = &m.createBox(top, seconds(4.), seconds(12.), 0.05);
    sub = safe_cast<Scenario::ProcessModel*>(m.createProcess(
        *b2, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, {}));
    b3 = &m.createBox(*sub, seconds(0.5), seconds(4.), 0.1);
    a3 = m.createProcess(*b3, automationKey, {}, {});
    m.commit();
    REQUIRE(a1);
    REQUIRE(a3);
    settle();
  }

  static void settle()
  {
    for(int i = 0; i < 5; i++)
    {
      QApplication::processEvents();
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
  }
};

//! A layer of a process, outside of any document view: what the focus points
//! to when the user clicks in a process.
struct Layer
{
  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx;
  QGraphicsScene scene;
  QGraphicsRectItem root{QRectF{0., 0., 1000., 400.}};
  Process::LayerView* view{};
  Process::LayerPresenter* presenter{};

  Layer(score::Document& doc, Process::ProcessModel& proc)
      : pctx{doc.context(), dfm, fd}
  {
    scene.addItem(&root);
    auto& layers = doc.context().app.interfaces<Process::LayerFactoryList>();
    auto* fact = layers.findDefaultFactory(proc);
    REQUIRE(fact);
    view = fact->makeLayerView(proc, pctx, &root);
    presenter = fact->makeLayerPresenter(proc, view, pctx, nullptr);
    REQUIRE(presenter);
  }

  ~Layer()
  {
    delete presenter;
    scene.removeItem(&root);
  }
};

std::unique_ptr<QMimeData> copy(score::Document& doc, IdentifiedObjectAbstract* obj)
{
  auto& ctx = doc.context();
  ctx.selectionStack.pushNewSelection(Selection{obj});
  JSONReader r;
  bool copied = false;
  for(auto& iface : ctx.app.interfaces<score::ObjectEditorList>())
    if((copied = iface.copy(r, ctx.selectionStack.currentSelection(), ctx)))
      break;
  REQUIRE(copied);
  auto mime = std::make_unique<QMimeData>();
  mime->setData("text/plain", QByteArray{r.buffer.GetString(), (int)r.buffer.GetSize()});
  return mime;
}

void focus(score::Document& doc, Process::LayerPresenter* p)
{
  auto fm = Process::ProcessFocusManager::get(doc.context());
  REQUIRE(fm);
  if(p)
    fm->focus(QPointer<Process::LayerPresenter>{p});
  else
    fm->focusNothing();
}

bool paste(score::Document& doc, const QMimeData& mime)
{
  auto& ctx = doc.context();
  auto fm = Process::ProcessFocusManager::get(ctx);
  QObject* focused = fm ? fm->focusedPresenter() : nullptr;
  // Away from every view: where the pointer is gives no target.
  const QPoint pos{-100000, -100000};
  for(auto& iface : ctx.app.interfaces<score::ObjectEditorList>())
    if(iface.paste(pos, focused, mime, ctx))
      return true;
  return false;
}

int processCount(const Scenario::IntervalModel& itv)
{
  return int(itv.processes.size());
}
}

TEST_CASE(
    "the play bar is drawn over the interval view shown after a nodal round trip",
    "[integration][scenario][timebar][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};
    n.b2->setViewMode(Scenario::IntervalModel::ViewMode::Nodal);

    auto& gv = n.pres.view().view();
    n.pres.startTimeBar();
    Nested::settle();
    REQUIRE(gv.currentView);

    // Without letting the event loop run in between, as when the second
    // navigation comes before the play bar is set up again.
    n.pres.setDisplayedInterval(n.b2);
    n.pres.setDisplayedInterval(&n.base);

    auto itv_pres = n.pres.displayedIntervalPresenter();
    REQUIRE(itv_pres);
    CHECK(gv.currentView == itv_pres->view());

    QImage img{400, 300, QImage::Format_ARGB32_Premultiplied};
    {
      QPainter p{&img};
      gv.render(&p);
    }

    Nested::settle();
    CHECK(gv.currentView == n.pres.displayedIntervalPresenter()->view());
    n.pres.stopTimeBar();
  });
}

TEST_CASE(
    "a process pasted with a nested process focused goes in its interval",
    "[integration][scenario][paste][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};
    Layer a3{*doc, *n.a3};

    auto mime = copy(*doc, n.a1);
    focus(*doc, a3.presenter);
    REQUIRE(paste(*doc, *mime));
    Nested::settle();

    CHECK(processCount(*n.b3) == 2);
    CHECK(processCount(*n.b1) == 1);
    CHECK(processCount(n.base) == 1);

    doc->commandStack().undo();
    Nested::settle();
    CHECK(processCount(*n.b3) == 1);
    focus(*doc, nullptr);
  });
}

TEST_CASE(
    "a process pasted after selecting one goes next to it",
    "[integration][scenario][paste][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};
    Layer a3{*doc, *n.a3};

    focus(*doc, a3.presenter);
    // The selection changes after the focus did.
    auto mime = copy(*doc, n.a1);
    REQUIRE(paste(*doc, *mime));
    Nested::settle();

    CHECK(processCount(*n.b1) == 2);
    CHECK(processCount(*n.b3) == 1);
    focus(*doc, nullptr);
  });
}

TEST_CASE(
    "an interval pasted with a nested process focused goes in its scenario",
    "[integration][scenario][paste][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};
    Layer sub{*doc, *n.sub};
    Layer a3{*doc, *n.a3};

    // Copying intervals goes through the scenario that has the focus, as when
    // one is clicked in it.
    focus(*doc, sub.presenter);
    auto mime = copy(*doc, n.b3);
    focus(*doc, a3.presenter);
    REQUIRE(paste(*doc, *mime));
    Nested::settle();

    REQUIRE(n.sub->intervals.size() == 2);
    Scenario::IntervalModel* pasted{};
    for(auto& itv : n.sub->intervals)
      if(&itv != n.b3)
        pasted = &itv;
    REQUIRE(pasted);
    // Next to the interval of the focused process
    CHECK(pasted->date() == n.b3->date());
    CHECK(pasted->heightPercentage() > n.b3->heightPercentage());
    CHECK(pasted->heightPercentage() < 1.);
    CHECK(n.top.intervals.size() == 2);

    doc->commandStack().undo();
    Nested::settle();
    CHECK(n.sub->intervals.size() == 1);
    focus(*doc, nullptr);
  });
}

TEST_CASE(
    "a process pasted with nothing focused or selected goes in the shown interval",
    "[integration][scenario][paste][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};

    auto mime = copy(*doc, n.a1);
    doc->context().selectionStack.pushNewSelection(Selection{});
    focus(*doc, nullptr);
    REQUIRE(paste(*doc, *mime));
    Nested::settle();

    CHECK(processCount(n.base) == 2);
    CHECK(processCount(*n.b1) == 1);
  });
}

TEST_CASE(
    "a process pasted after clicking an interval's nodal slot goes in that interval",
    "[integration][scenario][paste][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Nested n{*doc};

    // The nodal slot of b1, as its interval presenter makes it.
    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsScene scene;
    auto* slot = new Scenario::NodalIntervalView{
        Scenario::NodalIntervalView::OnlyEffects, *n.b1, pctx, nullptr};
    slot->setRect({0., 0., 800., 200.});
    scene.addItem(slot);

    auto mime = copy(*doc, n.a3);
    {
      QGraphicsSceneMouseEvent press{QEvent::GraphicsSceneMousePress};
      press.setButton(Qt::LeftButton);
      press.setButtons(Qt::LeftButton);
      press.setPos({700., 150.});
      press.setScenePos({700., 150.});
      scene.sendEvent(slot, &press);
    }
    REQUIRE(paste(*doc, *mime));
    Nested::settle();

    CHECK(processCount(*n.b1) == 2);
    CHECK(processCount(n.base) == 1);
    CHECK(processCount(*n.b3) == 1);

    scene.removeItem(slot);
    delete slot;
    focus(*doc, nullptr);
  });
}
