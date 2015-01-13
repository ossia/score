#include <QtTest/QtTest>
#include "Commands/Scenario/ShowBoxInViewModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Scenario/CreateEvent.hpp"

#include "Process/ScenarioProcessFactory.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/AbstractScenarioProcessViewModel.hpp"

#include "Control/ProcessList.hpp"


using namespace iscore;
using namespace Scenario::Command;

class ShowBoxInViewModelTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void test()
		{
			QUndoStack stack;
			// Maybe do a fake process list, with a fake process for unit tests.
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			// Setup
			ConstraintModel* constraint  = new ConstraintModel{0, qApp};

			// Creation of a scenario with a constraint
			auto cmd_proc = new AddProcessToConstraint (
			{
				{"ConstraintModel", {}}
			}, "Scenario");
			stack.push(cmd_proc);
			auto scenarioId = cmd_proc->m_createdProcessId;
			auto scenario = static_cast<ScenarioProcessSharedModel*>(constraint->process(scenarioId));


			// Creation of a way to visualize what happens in the original constraint
			auto cmd_box = new AddBoxToConstraint(
			ObjectPath{ {"ConstraintModel", {}} });
			stack.push(cmd_box);
			auto boxId = cmd_box->m_createdBoxId;

			auto cmd_deck = new AddDeckToBox(
			ObjectPath{
				{"ConstraintModel", {}},
				{"BoxModel", boxId}});
			auto deckId = cmd_deck->m_createdDeckId;
			stack.push(cmd_deck);

			auto cmd_pvm = new AddProcessViewModelToDeck(
			{{"ConstraintModel", {}},
			 {"BoxModel", boxId},
			 {"DeckModel", deckId}}, scenarioId);
			stack.push(cmd_pvm);

			auto viewmodel = constraint->boxes().front()->decks().front()->processViewModels().front();
			auto scenario_viewmodel = dynamic_cast<AbstractScenarioProcessViewModel*>(viewmodel);
			// Put this in the tests for AbstractScenarioProcessViewModel
			QVERIFY(scenario_viewmodel != nullptr);
			QCOMPARE(scenario_viewmodel->constraints().count(), 0);


			// Creation of an even and a constraint inside the scenario
			EventData data{};
			// data.id = 0; unused here
			data.dDate = 10;
			data.relativeY = 0.5;

			auto cmd_event = new CreateEvent(
			{
				{"ScenarioProcessSharedModel", {}},
			}, data.dDate, data.relativeY);
			stack.push(cmd_event);

			// This will create a view model for this constraint
			// in the previously-created Scenario View Model
			QCOMPARE(scenario_viewmodel->constraints().count(), 1);

			// Check that the constraint view model is properly instantiated
			AbstractConstraintViewModel* constraint_viewmodel = scenario_viewmodel->constraints().front();
			QCOMPARE(constraint_viewmodel->model(), scenario->constraint(cmd_event->m_cmd->m_createdConstraintId));
			QCOMPARE(constraint_viewmodel->isBoxShown(), false); // No box can be shown since there isn't any in this constraint

			auto cmd_box2 = new AddBoxToConstraint(
						ObjectPath{
									{"ConstraintModel", {}},
									{"ScenarioProcessSharedModel", scenarioId},
									{"ConstraintModel", cmd_event->m_cmd->m_createdConstraintId}
								  });
			stack.push(cmd_box2);

			QCOMPARE(constraint_viewmodel->isBoxShown(), false); // Now there is a box but we do not show it
			auto box2Id = cmd_box2->m_createdBoxId;


			// Show the box
			auto cmd_showbox = new ShowBoxInViewModel(ObjectPath::pathFromObject(constraint_viewmodel), box2Id);
			stack.push(cmd_showbox);
			QCOMPARE(constraint_viewmodel->isBoxShown(), true);
			QCOMPARE(constraint_viewmodel->shownBox(), box2Id);
		}
};

QTEST_MAIN(ShowBoxInViewModelTest)
#include "ShowBoxInViewModelTest.moc"


