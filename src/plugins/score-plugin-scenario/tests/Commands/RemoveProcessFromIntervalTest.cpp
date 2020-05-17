// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class RemoveProcessFromIntervalTest : public QObject
{
  Q_OBJECT

private:
  void DeleteCommandTest()
  {
    NamedObject* obj = new NamedObject("obj", qApp);
    ProcessList plist(obj);
    plist.registerProcess(new ScenarioFactory);

    IntervalModel* int_model
        = new IntervalModel{Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};
    int_model->createRack(Id<RackModel>{656});
    IntervalModel* int_model2
        = new IntervalModel{Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, int_model};
    int_model2->createRack(Id<RackModel>{656});

    QVERIFY(int_model2->processes().size() == 0);
    AddProcessToInterval cmd({{"IntervalModel", {}}, {"IntervalModel", 0}}, "Scenario");
    cmd.redo(ctx);
    QVERIFY(int_model2->processes().size() == 1);

    auto s0 = static_cast<Scenario::ProcessModel*>(int_model2->processes().front());

    auto int_0_id = getStrongId(s0->intervals());
    auto ev_0_id = getStrongId(s0->events());
    auto fv_0_id = Id<IntervalViewModel>{234};
    auto tb_0_id = getStrongId(s0->timeSyncs());
    StandardCreationPolicy::createIntervalAndEndEventFromEvent(
        *s0,
        s0->startEvent()->id(),
        std::chrono::milliseconds{34},
        10,
        int_0_id,
        fv_0_id,
        ev_0_id);
    s0->interval(int_0_id)->createRack(Id<RackModel>{5676});
    QCOMPARE((int)s0->intervals().size(), 1);
    QCOMPARE((int)s0->events().size(), 3);

    AddProcessToInterval cmd2(
        {{"IntervalModel", {}},
         {"IntervalModel", 0},
         {"ScenarioModel", s0->id()},
         {"IntervalModel", int_0_id}},
        "Scenario");

    cmd2.redo(ctx);
    QVERIFY(int_model2->processes().size() == 1);
    auto last_interval = s0->intervals().front();
    QVERIFY(last_interval->processes().size() == 1);

    RemoveProcessFromInterval cmd3({{"IntervalModel", {}}, {"IntervalModel", 0}}, s0->id());

    cmd3.redo(ctx);
    QVERIFY(int_model2->processes().size() == 0);
    cmd3.undo(ctx);
    QVERIFY(int_model2->processes().size() == 1);
    cmd3.redo(ctx);
    QVERIFY(int_model2->processes().size() == 0);
  }
};

QTEST_MAIN(RemoveProcessFromIntervalTest)
#include "RemoveProcessFromIntervalTest.moc"
