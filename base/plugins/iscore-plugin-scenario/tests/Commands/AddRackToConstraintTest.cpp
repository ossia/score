// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>

using namespace iscore;
using namespace Scenario::Command;

class AddRackToConstraintTest : public QObject
{
  Q_OBJECT

private slots:
  void CreateRackTest()
  {
    ConstraintModel* constraint = new ConstraintModel{
        Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, qApp};

    QCOMPARE((int)constraint->rackes().size(), 0);
    AddRackToConstraint cmd(ObjectPath{{"ConstraintModel", {0}}});

    auto id = cmd.m_createdRackId;

    cmd.redo(ctx);
    QCOMPARE((int)constraint->rackes().size(), 1);
    QCOMPARE(constraint->rack(id)->parent(), constraint);

    cmd.undo(ctx);
    QCOMPARE((int)constraint->rackes().size(), 0);

    cmd.redo(ctx);
    QCOMPARE((int)constraint->rackes().size(), 1);
    QCOMPARE(constraint->rack(id)->parent(), constraint);

    // Delete them else they stay in qApp !
    delete constraint;
  }
};

QTEST_MAIN(AddRackToConstraintTest)
#include "AddRackToConstraintTest.moc"
