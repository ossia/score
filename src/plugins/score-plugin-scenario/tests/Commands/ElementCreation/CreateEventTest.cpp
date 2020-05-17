// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class CreateEventTest : public QObject
{
  Q_OBJECT
public:
private:
  void CreateTest()
  {
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);
    EventData data{};
    // data.id = 0; unused here
    data.dDate.setMSecs(10);
    data.relativeY = 0.2;
    data.endTimeSyncId = Id<TimeSyncModel>(-1);

    CreateEvent cmd(
        {
            {"ScenarioModel", {0}},
        },
        data);

    cmd.redo(ctx);

    auto event = scenar->event(cmd.createdEvent());

    QCOMPARE((int)scenar->events().size(), 3);
    QCOMPARE(event->heightPercentage(), 0.2);
    QCOMPARE(cmd.createdTimeSync(), event->timeSync());

    cmd.undo(ctx);
    QCOMPARE((int)scenar->events().size(), 2);
    scenar->event(Id<EventModel>(0));
    scenar->event(Id<EventModel>(1));

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), 3);
    QCOMPARE(event->heightPercentage(), 0.2);

    // Delete them else they stay in qApp !

    delete scenar;
  }
  void CreateOnTimeSyncTest()
  {
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);
    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.6;
    data.endTimeSyncId = Id<TimeSyncModel>(-1);

    CreateEvent cmd1(
        {
            {"ScenarioModel", {0}},
        },
        data);
    cmd1.redo(ctx);

    data.endTimeSyncId = cmd1.createdTimeSync();
    data.relativeY = 0.2;

    CreateEvent cmd(
        {
            {"ScenarioModel", {0}},
        },
        data);

    int eventCount = 4;
    int timeSyncCount = 3;

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), eventCount);
    QCOMPARE((int)scenar->timeSyncs().size(), timeSyncCount);
    QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.2);

    cmd.undo(ctx);
    QCOMPARE((int)scenar->events().size(), eventCount - 1);
    QCOMPARE((int)scenar->timeSyncs().size(), timeSyncCount);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), eventCount);
    QCOMPARE((int)scenar->timeSyncs().size(), timeSyncCount);
    QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.2);

    // Delete them else they stay in qApp !

    delete scenar;
  }
};

QTEST_MAIN(CreateEventTest)
#include "CreateEventTest.moc"
