// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <QtTest/QtTest>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/AddRackToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>

using namespace score;
using namespace Scenario::Command;

class AddProcessToIntervalTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void CreateCommandTest()
  {
    NamedObject* obj = new NamedObject{"obj", qApp};
    ProcessList* plist = new ProcessList{obj};
    plist->registerProcess(new ScenarioFactory);

    IntervalModel* cstrModel = new IntervalModel{
        Id<IntervalModel>{1}, Id<IntervalViewModel>{0}, qApp};

    AddRackToInterval rackCmd(ObjectPath{{"IntervalModel", {1}}});
    rackCmd.redo(ctx);

    AddProcessToInterval cmd{{{"IntervalModel", {1}}}, "Scenario"};

    cmd.redo(ctx);
    QCOMPARE((int)cstrModel->processes().size(), 1);
    cmd.undo(ctx);
    QCOMPARE((int)cstrModel->processes().size(), 0);
    cmd.redo(ctx);
    QCOMPARE((int)cstrModel->processes().size(), 1);

    // Delete them else they stay in qApp !
    delete cstrModel;
    delete obj;
  }
};

QTEST_MAIN(AddProcessToIntervalTest)
#include "AddProcessToIntervalTest.moc"
