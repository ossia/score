#include <QtTest/QtTest>
#include <Scenario/Commands/Constraint/RemoveRackFromConstraint.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>

using namespace iscore;
using namespace Scenario::Command;

class RemoveRackFromConstraintTest: public QObject
{
        Q_OBJECT
    public:

    private slots:
        void test()
        {
            ConstraintModel* constraint  = new ConstraintModel {Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, qApp};

            AddRackToConstraint cmd
            {
                ObjectPath{ {"ConstraintModel", {}} }
            };

            auto id = cmd.m_createdRackId;
            cmd.redo();


            RemoveRackFromConstraint cmd2
            {
                ObjectPath{ {"ConstraintModel", {}} },
                id
            };
            cmd2.redo();
            QCOMPARE((int) constraint->rackes().size(), 0);
            cmd2.undo();
            QCOMPARE((int) constraint->rackes().size(), 1);
            cmd.undo();
            QCOMPARE((int) constraint->rackes().size(), 0);
            cmd.redo();
            cmd2.redo();

            // Delete them else they stay in qApp !
            delete constraint;
        }
};

QTEST_MAIN(RemoveRackFromConstraintTest)
#include "RemoveRackFromConstraintTest.moc"


