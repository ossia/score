#include <QtTest/QtTest>

#include <Commands/Scenario/MoveConstraint.hpp>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/ConstraintData.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

#include <Process/Algorithms/StandardCreationPolicy.hpp>
#include <Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

class MoveConstraintTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void MoveCommandTest()
        {
            ScenarioModel* scenar = new ScenarioModel(id_type<ProcessSharedModelInterface> {0}, qApp);

            auto int_0_id = getStrongId(scenar->constraints());
            auto ev_0_id = getStrongId(scenar->events());

            auto fv_0_id = id_type<AbstractConstraintViewModel> {234};
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




