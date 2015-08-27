#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Rack/RackModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <Document/TimeNode/TimeNodeModel.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Process/Algorithms/StandardCreationPolicy.hpp>


class ConstraintModelTests: public QObject
{
        Q_OBJECT
    public:
        ConstraintModelTests() : QObject {}
        {
        }

    private slots:

        void CreateSlotTest()
        {
            ConstraintModel model {Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, this};
            auto content_id = getStrongId(model.rackes());
            model.createRack(content_id);
            auto rack = model.rack(content_id);
            QVERIFY(rack != nullptr);

            auto slot_id = getStrongId(rack->getSlots());
            rack->addSlot(new SlotModel
            {
                slot_id,
                rack
            });
            auto slot = rack->slot(slot_id);
            QVERIFY(slot != nullptr);
        }

        void DeleteSlotTest()
        {
            /////
            {
                ConstraintModel model {Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, this};
                auto content_id = getStrongId(model.rackes());
                model.createRack(content_id);
                auto rack = model.rack(content_id);

                auto slot_id = getStrongId(rack->getSlots());
                rack->addSlot(new SlotModel
                {
                    slot_id,
                    rack
                });
                rack->removeSlot(slot_id);
                model.removeRack(content_id);
            }

            //////
            {
                ConstraintModel model {Id<ConstraintModel>{0},
                                       Id<ConstraintViewModel>{0}, this
                                      };
                auto content_id = getStrongId(model.rackes());
                model.createRack(content_id);
                auto rack = model.rack(content_id);

                rack->addSlot(new SlotModel
                {
                    getStrongId(rack->getSlots()),
                    rack
                });

                rack->addSlot(new SlotModel
                {
                    getStrongId(rack->getSlots()),
                    rack
                });

                rack->addSlot(new SlotModel
                {
                    getStrongId(rack->getSlots()),
                    rack
                });

                model.removeRack(content_id);
            }
        }

        void FindSubProcessTest()
        {
            ConstraintModel i0 {Id<ConstraintModel>{0},
                                Id<ConstraintViewModel>{0}, qApp
                               };
            i0.setObjectName("OriginalConstraint");
            auto s0 = new ScenarioModel {std::chrono::seconds(15), Id<ProcessModel>{0}, &i0};

            auto int_0_id = getStrongId(s0->constraints());
            auto ev_0_id = getStrongId(s0->events());
            auto fv_0_id = Id<ConstraintViewModel> {234};
            auto tb_0_id = getStrongId(s0->timeNodes());
            StandardCreationPolicy::createConstraintAndEndEventFromEvent(*s0, s0->startEvent()->id(), std::chrono::milliseconds {34}, 10, int_0_id, fv_0_id, ev_0_id);

            auto int_2_id = getStrongId(s0->constraints());
            auto fv_2_id = Id<ConstraintViewModel> {454};
            auto ev_2_id = getStrongId(s0->events());
            auto tb_2_id = getStrongId(s0->timeNodes());
            StandardCreationPolicy::createConstraintAndEndEventFromEvent(*s0, s0->startEvent()->id(), std::chrono::milliseconds {46}, 10, int_2_id, fv_2_id, ev_2_id);

            auto i1 = s0->constraint(int_0_id);
            auto s1 = new ScenarioModel {std::chrono::seconds(15), Id<ProcessModel>{0}, i1};
            (void) s1;
            auto s2 = new ScenarioModel {std::chrono::seconds(15), Id<ProcessModel>{1}, i1};

            ObjectPath p
            {
                {"OriginalConstraint", {0}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 1}
            };
            QCOMPARE(p.find<QObject>(), s2);

            ObjectPath p2
            {
                {"OriginalConstraint", {0}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 7}
            };

            try
            {
                p2.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }

            ObjectPath p3
            {
                {"OriginalConstraint", {0}},
                {"ScenarioModel", 0},
                {"ConstraintModel0xBADBAD", int_0_id},
                {"ScenarioModel", 1}
            };

            try
            {
                p3.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }

            ObjectPath p4
            {
                {"OriginalConstraint", {0}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 1},
                {"ScenarioModel", 1}
            };

            try
            {
                p4.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }
        }

};

QTEST_MAIN(ConstraintModelTests)
#include "ConstraintModelTests.moc"

