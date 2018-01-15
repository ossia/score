// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>

#include <Scenario/Commands/Scenario/Creations/CreateEventAfterEvent.hpp>

#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

using namespace score;
using namespace Scenario::Command;

class CreateEventAfterEventTest : public QObject
{
  Q_OBJECT
public:
private Q_SLOTS:
  void CreateTest()
  {
    Scenario::ProcessModel* scenar = new ScenarioModel(
        std::chrono::seconds(15), Id<ProcessModel>{0}, qApp);

    CreateEventAfterEvent cmd(
        {
            {"ScenarioModel", {0}},
        },
        scenar->startEvent()->id(), TimeValue::fromMsecs(10), 0.5);

    cmd.redo(ctx);
    QCOMPARE((int)scenar->events().size(), 2);
    QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);

    cmd.undo(ctx);
    QCOMPARE((int)scenar->events().size(), 1);

    try
    {
      scenar->event(cmd.m_createdEventId);
      QFAIL("Event call did not throw!");
    }
    catch (...)
    {
    }

    cmd.redo(ctx);

    QCOMPARE((int)scenar->events().size(), 2);
    QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);

    // Delete them else they stay in qApp !

    delete scenar;
  }
};

QTEST_MAIN(CreateEventAfterEventTest)
#include "CreateEventAfterEventTest.moc"
