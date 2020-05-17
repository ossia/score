// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "QtTest/QTest"

#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class CreateIntervalTest : public QObject
{
  Q_OBJECT
private:
  void RemoveTest()
  {
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.8;
    data.endTimeSyncId = Id<TimeSyncModel>(-1);

    CreateEventAfterEvent eventCmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    eventCmd.redo(ctx);

    data.dDate.setMSecs(30);
    data.relativeY = 0.4;

    CreateEvent event2Cmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    event2Cmd.redo(ctx);

    auto firstEvent_id = eventCmd.createdEvent();
    auto lastEvent_id = event2Cmd.createdEvent();

    auto firstEvent = scenar->event(firstEvent_id);
    auto lastEvent = scenar->event(lastEvent_id);

    CreateInterval cstrCmd(
        {
            {"ScenarioModel", {0}},
        },
        firstEvent_id,
        lastEvent_id);

    cstrCmd.redo(ctx);

    QCOMPARE(firstEvent->nextIntervals().size(), 1);
    QCOMPARE(lastEvent->previousIntervals().size(), 2);

    QCOMPARE(firstEvent->nextIntervals().at(0), cstrCmd.createdInterval());

    cstrCmd.undo(ctx);

    QCOMPARE(firstEvent->nextIntervals().size(), 0);
    QCOMPARE(lastEvent->previousIntervals().size(), 1);

    QCOMPARE(lastEvent->previousIntervals().indexOf(cstrCmd.createdInterval()), -1);
    QCOMPARE(firstEvent->nextIntervals().indexOf(cstrCmd.createdInterval()), -1);

    try
    {
      scenar->interval(cstrCmd.createdInterval());
      QFAIL("Interval call did not throw !");
    }
    catch (...)
    {
    }
    cstrCmd.redo(ctx);

    QCOMPARE(firstEvent->nextIntervals().size(), 1);
    QCOMPARE(lastEvent->previousIntervals().size(), 2);

    QCOMPARE(firstEvent->nextIntervals().at(0), cstrCmd.createdInterval());
  }
};

QTEST_MAIN(CreateIntervalTest)
#include "CreateIntervalTest.moc"
