#include "QtTest/QTest"

#include "Commands/Scenario/Creations/CreateConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include <Document/Event/EventData.hpp>
#include "Process/ScenarioModel.hpp"


using namespace iscore;
using namespace Scenario::Command;

class CreateConstraintTest: public QObject
{
        Q_OBJECT
    private slots:
        void RemoveTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), id_type<ProcessSharedModelInterface> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.8;
            data.endTimeNodeId = id_type<TimeNodeModel>(-1);

            CreateEventAfterEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            data.dDate.setMSecs(30);
            data.relativeY = 0.4;

            CreateEvent event2Cmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            event2Cmd.redo();

            auto firstEvent_id = eventCmd.createdEvent();
            auto lastEvent_id = event2Cmd.createdEvent();

            auto firstEvent = scenar->event(firstEvent_id);
            auto lastEvent = scenar->event(lastEvent_id);

            CreateConstraint cstrCmd (
            {
                {"ScenarioModel", {0}},
            }, firstEvent_id,
                lastEvent_id);

            cstrCmd.redo();

            QCOMPARE(firstEvent->nextConstraints().size(), 1);
            QCOMPARE(lastEvent->previousConstraints().size(), 2);

            QCOMPARE(firstEvent->nextConstraints().at(0), cstrCmd.createdConstraint());

            cstrCmd.undo();

            QCOMPARE(firstEvent->nextConstraints().size(), 0);
            QCOMPARE(lastEvent->previousConstraints().size(), 1);

            QCOMPARE(lastEvent->previousConstraints().indexOf(cstrCmd.createdConstraint()), -1);
            QCOMPARE(firstEvent->nextConstraints().indexOf(cstrCmd.createdConstraint()), -1);

            try
            {
                scenar->constraint(cstrCmd.createdConstraint());
                QFAIL("Constraint call did not throw !");
            }
            catch (...) { }
            cstrCmd.redo();

            QCOMPARE(firstEvent->nextConstraints().size(), 1);
            QCOMPARE(lastEvent->previousConstraints().size(), 2);

            QCOMPARE(firstEvent->nextConstraints().at(0), cstrCmd.createdConstraint());

        }
};

QTEST_MAIN(CreateConstraintTest)
#include "CreateConstraintTest.moc"
