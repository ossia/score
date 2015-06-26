#include <QtTest/QtTest>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Slot/SlotModel.hpp>

#include "Commands/Constraint/Box/AddSlotToBox.hpp"

using namespace iscore;
using namespace Scenario::Command;

class AddSlotToBoxTest: public QObject
{
        Q_OBJECT

    private slots:
        void CreateSlotTest()
        {
            BoxModel* box  = new BoxModel {id_type<BoxModel>{0}, qApp};

            QCOMPARE((int) box->getSlots().size(), 0);
            AddSlotToBox cmd(
            ObjectPath { {"BoxModel", {0}} });
            auto slotId = cmd.m_createdSlotId;

            cmd.redo();
            QCOMPARE((int) box->getSlots().size(), 1);
            QCOMPARE(box->slot(slotId)->parent(), box);

            cmd.undo();
            QCOMPARE((int) box->getSlots().size(), 0);

            cmd.redo();
            QCOMPARE((int) box->getSlots().size(), 1);
            QCOMPARE(box->slot(slotId)->parent(), box);

            try
            {
                box->slot(slotId);
            }
            catch(std::runtime_error& e)
            {
                QFAIL(e.what());
            }

            // Delete them else they stay in qApp !
            delete box;
        }
};

QTEST_MAIN(AddSlotToBoxTest)
#include "AddSlotToBoxTest.moc"

