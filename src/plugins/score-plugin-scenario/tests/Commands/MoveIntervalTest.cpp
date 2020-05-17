// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveInterval.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalData.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class MoveIntervalTest : public QObject
{
  Q_OBJECT
public:
private:
  void MoveCommandTest()
  {
    Scenario::ProcessModel* scenar
        = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    auto int_0_id = getStrongId(scenar->intervals());
    auto ev_0_id = getStrongId(scenar->events());

    auto fv_0_id = Id<IntervalViewModel>{234};
    auto tb_0_id = getStrongId(scenar->timeSyncs());
    StandardCreationPolicy::createIntervalAndEndEventFromEvent(
        *scenar,
        scenar->startEvent()->id(),
        std::chrono::milliseconds{34},
        0.5,
        int_0_id,
        fv_0_id,
        ev_0_id);

    IntervalData data{};
    data.id = int_0_id;
    data.relativeY = 0.1;
    MoveInterval cmd(
        {
            {"ScenarioModel", {}},
        },
        data);

    cmd.redo(ctx);
    QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.1);

    cmd.undo(ctx);
    QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.5);

    cmd.redo(ctx);
    QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.1);

    // Delete them else they stay in qApp !

    delete scenar;
  }
};

QTEST_MAIN(MoveIntervalTest)
#include "MoveIntervalTest.moc"
