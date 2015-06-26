#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Rack/RackModel.hpp>

#include "Commands/Constraint/AddRackToConstraint.hpp"

using namespace iscore;
using namespace Scenario::Command;

class AddRackToConstraintTest: public QObject
{
        Q_OBJECT

    private slots:
        void CreateRackTest()
        {
            ConstraintModel* constraint  = new ConstraintModel {id_type<ConstraintModel>{0},
                                                                id_type<AbstractConstraintViewModel>{0},
                                                                qApp
                                                               };

            QCOMPARE((int) constraint->rackes().size(), 0);
            AddRackToConstraint cmd(
            ObjectPath { {"ConstraintModel", {0}} });

            auto id = cmd.m_createdRackId;

            cmd.redo();
            QCOMPARE((int) constraint->rackes().size(), 1);
            QCOMPARE(constraint->rack(id)->parent(), constraint);

            cmd.undo();
            QCOMPARE((int) constraint->rackes().size(), 0);

            cmd.redo();
            QCOMPARE((int) constraint->rackes().size(), 1);
            QCOMPARE(constraint->rack(id)->parent(), constraint);

            // Delete them else they stay in qApp !
            delete constraint;
        }


};

QTEST_MAIN(AddRackToConstraintTest)
#include "AddRackToConstraintTest.moc"

