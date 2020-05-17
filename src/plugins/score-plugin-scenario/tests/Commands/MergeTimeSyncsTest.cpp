// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/TimeSync/MergeTimeSyncs.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentInterface.hpp>

using namespace score;
using namespace Scenario::Command;

class MergeTimeSyncsTest : public QObject
{
  Q_OBJECT
private:
  void mergeTest()
  {
    // A scenario, 2 events at same date
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

    auto event1_id = eventCmd.createdEvent();
    auto event1 = scenar->event(event1_id);
    auto tn1_id = eventCmd.createdTimeSync();

    data.relativeY = 0.4;

    CreateEvent event2Cmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    event2Cmd.redo(ctx);

    auto event2_id = event2Cmd.createdEvent();
    auto event2 = scenar->event(event2_id);
    auto tn2_id = event2Cmd.createdTimeSync();

    auto tn2 = scenar->timeSync(tn2_id);

    tn2->addEvent(event1_id);

    MergeTimeSyncs cmd(
        {
            {"ScenarioModel", {0}},
        },
        tn1_id,
        tn2_id);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->timeSyncs().size(), 3);

    QCOMPARE(event2->timeSync(), event1->timeSync());
    try
    {
      scenar->timeSync(tn2_id);
      QFAIL("TimeSync call did not throw");
    }
    catch (...)
    {
    }

    cmd.undo(ctx);
    QCOMPARE((int)scenar->timeSyncs().size(), 4);
    try
    {
      scenar->timeSync(tn2_id);
      QCOMPARE(scenar->timeSync(tn2_id)->events().first(), event2_id);
    }
    catch (...)
    {
      QFAIL("TimeSync call throw ...");
    }

    cmd.redo(ctx);
    QCOMPARE((int)scenar->timeSyncs().size(), 3);
    QCOMPARE(event2->timeSync(), event1->timeSync());
    try
    {
      scenar->timeSync(tn2_id);
      QFAIL("TimeSync call did not throw");
    }
    catch (...)
    {
    }
  }
};

QTEST_MAIN(MergeTimeSyncsTest)
#include "MergeTimeSyncsTest.moc"
