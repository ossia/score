// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "QtTest/QTest"

#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEventAfterEvent.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveInterval.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class RemoveIntervalTest : public QObject
{
  Q_OBJECT
private:
  void removeFirstIntervalTest()
  {
    // create scenar and 1 event
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.8;
    data.endTimeSyncId = Id<TimeSyncModel>(-1);

    CreateEvent eventCmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    eventCmd.redo(ctx);

    auto event_id = eventCmd.createdEvent();
    auto eventCreated = scenar->event(event_id);

    auto interval_id = eventCreated->previousIntervals().at(0);
    auto interval = scenar->interval(interval_id);

    auto y = interval->heightPercentage();

    RemoveInterval cmd(
        {
            {"ScenarioModel", {0}},
        },
        interval);
    cmd.redo(ctx);

    QCOMPARE((int)scenar->intervals().size(), 0);
    QCOMPARE((int)eventCreated->previousIntervals().size(), 0);

    cmd.undo(ctx);
    QCOMPARE((int)scenar->intervals().size(), 1);
    QCOMPARE((int)eventCreated->previousIntervals().size(), 1);
    QCOMPARE(scenar->intervals().at(0)->id(), interval_id);
    QCOMPARE(eventCreated->previousIntervals().at(0), interval_id);
    QCOMPARE(scenar->interval(interval_id)->heightPercentage(), y);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->intervals().size(), 0);
    QCOMPARE((int)eventCreated->previousIntervals().size(), 0);

    delete scenar;
  }
  void removeAnyIntervalTest()
  {
    // create scenar and 2 events
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.8;
    data.endTimeSyncId = Id<TimeSyncModel>(-1);

    CreateEvent eventCmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    eventCmd.redo(ctx);

    auto startEv_id = eventCmd.createdEvent();

    CreateEventAfterEvent event2Cmd(
        {
            {"ScenarioModel", {0}},
        },
        startEv_id,
        TimeValue::fromMsecs(30),
        0.5);
    event2Cmd.redo(ctx);

    auto endEvent_id = event2Cmd.createdEvent();

    auto startEventCreated = scenar->event(startEv_id);
    auto endEventCreated = scenar->event(endEvent_id);

    auto interval_id = endEventCreated->previousIntervals().at(0);
    auto interval = scenar->interval(interval_id);

    auto y = interval->heightPercentage();

    RemoveInterval cmd(
        {
            {"ScenarioModel", {0}},
        },
        interval);
    cmd.redo(ctx);

    QCOMPARE((int)scenar->intervals().size(), 1);
    QCOMPARE((int)endEventCreated->previousIntervals().size(), 0);
    QCOMPARE((int)startEventCreated->nextIntervals().size(), 0);

    cmd.undo(ctx);
    QCOMPARE((int)scenar->intervals().size(), 2);
    QCOMPARE((int)endEventCreated->previousIntervals().size(), 1);
    QCOMPARE((int)startEventCreated->nextIntervals().size(), 1);

    QCOMPARE(scenar->intervals().at(1)->id(), interval_id);
    QCOMPARE(endEventCreated->previousIntervals().at(0), interval_id);
    QCOMPARE(startEventCreated->nextIntervals().at(0), interval_id);

    QCOMPARE(scenar->interval(interval_id)->heightPercentage(), y);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->intervals().size(), 1);
    QCOMPARE((int)endEventCreated->previousIntervals().size(), 0);
    QCOMPARE((int)startEventCreated->nextIntervals().size(), 0);

    delete scenar;
  }
};

QTEST_MAIN(RemoveIntervalTest)
#include "RemoveIntervalTest.moc"
