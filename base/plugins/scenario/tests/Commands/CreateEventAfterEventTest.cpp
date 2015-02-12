#include <QtTest/QtTest>

#include <Commands/Scenario/CreateEventAfterEvent.hpp>

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
		void CreateTest()
		{
			ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(id_type<ProcessSharedModelInterface>{0}, qApp);
			EventData data{};
			data.eventClickedId = scenar->startEvent()->id();
			data.dDate.setMSecs(10);
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


