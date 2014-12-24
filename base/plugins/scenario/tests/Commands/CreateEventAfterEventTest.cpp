#include <QtTest/QtTest>

#include <Commands/Scenario/CreateEventAfterEvent.hpp>
#include <Commands/Scenario/CreateEvent.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class CreateEventAfterEventTest: public QObject
{
		Q_OBJECT
	public:

	private slots:

		void CreateCommandTest()
		{
			ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);
			EventData data{};
			// data.id = 0; unused here
			data.x = 10;
			data.relativeY = 0.5;

			CreateEvent cmd(
			{
				{"ScenarioProcessSharedModel", {}},
			}, data.x, data.relativeY);

			cmd.redo();
			QCOMPARE((int)scenar->events().size(), 2); // TODO 3 if endEvent
			QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);

			cmd.undo();
			QCOMPARE((int)scenar->events().size(), 1); // TODO 2 if endEvent
			cmd.redo();

			QCOMPARE((int)scenar->events().size(), 2);
			QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);


			// Delete them else they stay in qApp !

			delete scenar;
		}

		void CreateAfterCommandTest()
		{
			ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);
			EventData data{};
			data.eventClickedId = scenar->startEvent()->id();
			data.x = 10;
			data.relativeY = 0.5;

			CreateEventAfterEvent cmd(
			{
				{"ScenarioProcessSharedModel", {}},
			}, data);

			cmd.redo();
			QCOMPARE((int)scenar->events().size(), 2);
			QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);

			cmd.undo();
			QCOMPARE((int)scenar->events().size(), 1);
			try
			{
				scenar->event(cmd.m_createdEventId);
				QFAIL("Event call did not throw!");
			}
			catch(...) { }

			cmd.redo();

			QCOMPARE((int)scenar->events().size(), 2);
			QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);


			// Delete them else they stay in qApp !

			delete scenar;
		}
};

QTEST_MAIN(CreateEventAfterEventTest)
#include "CreateEventAfterEventTest.moc"


