// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "QtTest/QTest"

#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

class CreateConstraintTest : public QObject
{
  Q_OBJECT
private slots:
  void RemoveTest()
  {
    Scenario::ProcessModel* scenar = new ScenarioModel(
        std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    EventData data{};
    data.dDate.setMSecs(10);
    data.relativeY = 0.8;
    data.endTimeNodeId = Id<TimeNodeModel>(-1);

    CreateEventAfterEvent eventCmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    eventCmd.redo(ctx);

    data.dDate.setMSecs(30);
    data.relativeY = 0.4;

    CreateEvent event2Cmd(
        {
            {"ScenarioModel", {0}},
        },
        data);
    event2Cmd.redo(ctx);

    auto firstEvent_id = eventCmd.createdEvent();
    auto lastEvent_id = event2Cmd.createdEvent();

    auto firstEvent = scenar->event(firstEvent_id);
    auto lastEvent = scenar->event(lastEvent_id);

    CreateConstraint cstrCmd(
        {
            {"ScenarioModel", {0}},
        },
        firstEvent_id, lastEvent_id);

    cstrCmd.redo(ctx);

    QCOMPARE(firstEvent->nextConstraints().size(), 1);
    QCOMPARE(lastEvent->previousConstraints().size(), 2);

    QCOMPARE(firstEvent->nextConstraints().at(0), cstrCmd.createdConstraint());

    cstrCmd.undo(ctx);

    QCOMPARE(firstEvent->nextConstraints().size(), 0);
    QCOMPARE(lastEvent->previousConstraints().size(), 1);

    QCOMPARE(
        lastEvent->previousConstraints().indexOf(cstrCmd.createdConstraint()),
        -1);
    QCOMPARE(
        firstEvent->nextConstraints().indexOf(cstrCmd.createdConstraint()),
        -1);

    try
    {
      scenar->constraint(cstrCmd.createdConstraint());
      QFAIL("Constraint call did not throw !");
    }
    catch (...)
    {
    }
    cstrCmd.redo(ctx);

    QCOMPARE(firstEvent->nextConstraints().size(), 1);
    QCOMPARE(lastEvent->previousConstraints().size(), 2);

    QCOMPARE(firstEvent->nextConstraints().at(0), cstrCmd.createdConstraint());
  }
};

QTEST_MAIN(CreateConstraintTest)
#include "CreateConstraintTest.moc"
