// Regression for Interval.rood11 in timemodel-patterns.score: snapping a
// duration as a scenario date jumped Min past the interval on the first move,
// then subsequent command updates clamped it to the endpoint.

#include <Scenario/Commands/Interval/SetMaxDuration.hpp>
#include <Scenario/Commands/Interval/SetMinDuration.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>

#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

TEST_CASE("Minimum brace follows a small leftward drag", "[integration][scenario][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    settings.setMeasureBars(true);
    settings.setMagneticMeasures(true);
    auto* source = score::test::new_document(ctx);
    REQUIRE(source);
    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(source->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());
    CommandDispatcher<> dispatcher{source->context().commandStack};
    auto* createStart = new Scenario::Command::CreateTimeSync_Event_State{
        scenario, TimeVal::fromMsecs(20750), 0.5};
    dispatcher.submit(createStart);
    auto* createInterval = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenario, createStart->createdState(), TimeVal::fromMsecs(23750), 0.5, false};
    dispatcher.submit(createInterval);
    auto& created = scenario.intervals.at(createInterval->createdInterval());
    created.metadata().setName(QStringLiteral("brace-drag"));
    created.duration.setRigid(false);
    created.duration.setMaxInfinite(true);

    // Reproduce a scrolled musical timeline: the interval duration is outside
    // the visible grid, but its scenario-relative endpoint is inside it.
    root.duration.setDefaultDuration(TimeVal::fromMsecs(30000));
    root.duration.setGuiDuration(TimeVal::fromMsecs(36000));
    root.setHasTimeSignature(true);
    root.addSignature(TimeVal::zero(), {4, 4});
    root.setZoom(TimeVal::fromMsecs(10).impl);
    root.setMidTime(TimeVal::fromMsecs(21500));
    auto* doc = score::test::reload_via_bytes(ctx, *source);
    REQUIRE(doc);
    ctx.docManager.forceCloseDocument(ctx, *source);
    QApplication::processEvents();
    QApplication::processEvents();
    auto* presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter);
    auto& scene = presenter->view().scene();
    Scenario::TemporalIntervalView* interval = nullptr;
    for(auto* item : scene.items())
    {
      if(auto* view = dynamic_cast<Scenario::TemporalIntervalView*>(item);
         view
         && view->presenter().model().metadata().getName()
                == QStringLiteral("brace-drag"))
        interval = view;
    }
    REQUIRE(interval);
    auto& brace = interval->leftBrace();
    REQUIRE(brace.isVisible());
    auto& model = interval->presenter().model();
    const auto initial = model.duration.minDuration();
    const auto start = brace.mapToScene(QPointF{5., 8.});
    auto send = [&](QEvent::Type type, QPointF at, QPointF last, Qt::MouseButton button,
                    Qt::MouseButtons buttons) {
      QGraphicsSceneMouseEvent ev{type};
      ev.setScenePos(at);
      ev.setLastScenePos(last);
      ev.setButtonDownScenePos(Qt::LeftButton, start);
      ev.setScreenPos(QPoint{700, 400});
      ev.setLastScreenPos(QPoint{701, 400});
      ev.setButtonDownScreenPos(Qt::LeftButton, QPoint{701, 400});
      ev.setButton(button);
      ev.setButtons(buttons);
      QCoreApplication::sendEvent(&scene, &ev);
      QApplication::processEvents();
      QApplication::processEvents();
    };
    send(QEvent::GraphicsSceneMousePress, start, start, Qt::LeftButton, Qt::LeftButton);
    REQUIRE(scene.mouseGrabberItem() == &brace);
    auto dest = start - QPointF{2., 0.};
    send(QEvent::GraphicsSceneMouseMove, dest, start, Qt::NoButton, Qt::LeftButton);
    INFO("initial " << initial.impl << " dragged " << model.duration.minDuration().impl);
    CHECK(model.duration.minDuration() <= initial);
    CHECK(model.duration.minDuration() >= TimeVal::zero());
    // Aim the endpoint at 22 s, not the duration at a beat relative to zero.
    const auto further
        = start
          - QPointF{
              TimeVal::fromMsecs(1750).toPixelsRaw(interval->presenter().zoomRatio()),
              0.};
    send(QEvent::GraphicsSceneMouseMove, further, dest, Qt::NoButton, Qt::LeftButton);
    CHECK(model.duration.minDuration() == TimeVal::fromMsecs(1250));
    const auto finalDuration = model.duration.minDuration();
    send(
        QEvent::GraphicsSceneMouseRelease, further, further, Qt::LeftButton,
        Qt::NoButton);
    doc->commandStack().undo();
    CHECK(model.duration.minDuration() == initial);
    doc->commandStack().redo();
    CHECK(model.duration.minDuration() == finalDuration);

    // The first submission uses the constructor; later ones use update().
    // Both must respect the same bounds and remain undoable.
    Scenario::Command::SetMinDuration belowZero{model, TimeVal::fromMsecs(-1000), false};
    belowZero.redo(doc->context());
    CHECK(model.duration.minDuration() == TimeVal::zero());
    belowZero.undo(doc->context());
    CHECK(model.duration.minDuration() == finalDuration);
    Scenario::Command::SetMinDuration aboveDefault{
        model, TimeVal::fromMsecs(10000), false};
    aboveDefault.redo(doc->context());
    CHECK(model.duration.minDuration() == model.duration.defaultDuration());
    aboveDefault.undo(doc->context());
    CHECK(model.duration.minDuration() == finalDuration);
    Scenario::Command::SetMaxDuration belowDefault{model, TimeVal::zero(), false};
    belowDefault.redo(doc->context());
    CHECK(model.duration.maxDuration() == model.duration.defaultDuration());
    belowDefault.undo(doc->context());
    CHECK(model.duration.isMaxInfinite());
  });
}
