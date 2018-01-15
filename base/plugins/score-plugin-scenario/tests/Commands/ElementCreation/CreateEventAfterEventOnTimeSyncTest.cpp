// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QTest>

#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEventAfterEventOnTimeSync.hpp>

#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class CreateEventAfterEventOnTimeSyncTest : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void CreateTest()
  {
    Scenario::ProcessModel* scenar = new ScenarioModel(
        std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.2;

    CreateEvent eventCmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    eventCmd.redo(ctx);

    auto endTn_id = eventCmd.createdTimeSync();

    data.eventClickedId = scenar->startEvent()->id();
    data.relativeY = 0.4;
    data.endTimeSyncId = endTn_id;

    CreateEventAfterEventOnTimeSync cmd(
        {
            {"ScenarioModel", {0}},
        },
        scenar->startEvent()->id(), eventCmd.createdTimeSync(),
        TimeValue::fromMsecs(10), 0.4);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), 3);
    QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.4);

    cmd.undo(ctx);
    QCOMPARE((int)scenar->events().size(), 2);
    QCOMPARE((int)scenar->timeSyncs().size(), 2);
    try
    {
      scenar->event(cmd.createdEvent());
      QFAIL("Event call did not throw!");
    }
    catch (...)
    {
    }

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), 3);
    QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.4);

    delete scenar;
  }
};

QTEST_MAIN(CreateEventAfterEventOnTimeSyncTest)
#include "CreateEventAfterEventOnTimeSyncTest.moc"
