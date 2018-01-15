// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>

#include <Scenario/Commands/Interval/AddRackToInterval.hpp>

using namespace score;
using namespace Scenario::Command;

class AddRackToIntervalTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void CreateRackTest()
  {
    IntervalModel* interval = new IntervalModel{
        Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};

    QCOMPARE((int)interval->rackes().size(), 0);
    AddRackToInterval cmd(ObjectPath{{"IntervalModel", {0}}});

    auto id = cmd.m_createdRackId;

    cmd.redo(ctx);
    QCOMPARE((int)interval->rackes().size(), 1);
    QCOMPARE(interval->rack(id)->parent(), interval);

    cmd.undo(ctx);
    QCOMPARE((int)interval->rackes().size(), 0);

    cmd.redo(ctx);
    QCOMPARE((int)interval->rackes().size(), 1);
    QCOMPARE(interval->rack(id)->parent(), interval);

    // Delete them else they stay in qApp !
    delete interval;
  }
};

QTEST_MAIN(AddRackToIntervalTest)
#include "AddRackToIntervalTest.moc"
