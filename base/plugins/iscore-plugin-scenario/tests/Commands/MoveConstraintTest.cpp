#include <QtTest/QtTest>

#include <Scenario/Commands/Scenario/Displacement/MoveConstraint.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ConstraintData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

class MoveConstraintTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void MoveCommandTest()
        {
            Scenario::ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            auto int_0_id = getStrongId(scenar->constraints());
            auto ev_0_id = getStrongId(scenar->events());

            auto fv_0_id = Id<ConstraintViewModel> {234};
            auto tb_0_id = getStrongId(scenar->timeNodes());
            StandardCreationPolicy::createConstraintAndEndEventFromEvent(*scenar, scenar->startEvent()->id(), std::chrono::milliseconds {34}, 0.5, int_0_id, fv_0_id, ev_0_id);

            ConstraintData data {};
            data.id = int_0_id;
            data.relativeY = 0.1;
            MoveConstraint cmd(
            {
                {"ScenarioModel", {}},
            }, data);

            cmd.redo();
            QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.1);

            cmd.undo();
            QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.5);

            cmd.redo();
            QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.1);


            // Delete them else they stay in qApp !

            delete scenar;
        }
};

QTEST_MAIN(MoveConstraintTest)
#include "MoveConstraintTest.moc"




