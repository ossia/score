// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <Scenario/Commands/Interval/RemoveRackFromInterval.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>

#include <Scenario/Commands/Interval/AddRackToInterval.hpp>

using namespace score;
using namespace Scenario::Command;

class RemoveRackFromIntervalTest : public QObject
{
  Q_OBJECT
public:
private Q_SLOTS:
  void test()
  {
    IntervalModel* interval = new IntervalModel{
        Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};

    AddRackToInterval cmd{ObjectPath{{"IntervalModel", {}}}};

    auto id = cmd.m_createdRackId;
    cmd.redo(ctx);

    RemoveRackFromInterval cmd2{ObjectPath{{"IntervalModel", {}}}, id};
    cmd2.redo(ctx);
    QCOMPARE((int)interval->rackes().size(), 0);
    cmd2.undo(ctx);
    QCOMPARE((int)interval->rackes().size(), 1);
    cmd.undo(ctx);
    QCOMPARE((int)interval->rackes().size(), 0);
    cmd.redo(ctx);
    cmd2.redo(ctx);

    // Delete them else they stay in qApp !
    delete interval;
  }
};

QTEST_MAIN(RemoveRackFromIntervalTest)
#include "RemoveRackFromIntervalTest.moc"
