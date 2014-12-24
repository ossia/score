#include <QtTest/QtTest>
#include "Commands/Scenario/HideBoxInViewModel.hpp"

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>

#include "Commands/Constraint/Box/Deck/AddProcessViewToDeck.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include <Process/ScenarioProcessFactory.hpp>
#include "Control/ProcessList.hpp"

#include <Commands/Scenario/CreateEvent.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

class HideBoxInViewModelTest: public QObject
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



			// Creation of an even and a constraint inside the scenario
			EventData data{};
			// data.id = 0; unused here
			data.x = 10;
			data.relativeY = 0.5;

			auto cmd_event = new CreateEvent(
			{
				{"ScenarioProcessSharedModel", {}},
			}, data.x, data.relativeY);
			stack.push(cmd_event);

			auto cmd_box2 = new AddBoxToConstraint(
						ObjectPath{
									{"ConstraintModel", {}},
									{"ScenarioProcessSharedModel", scenarioId},
									{"ConstraintModel", {}}
								  });
			stack.push(cmd_box2);
			auto box2Id = cmd_box2->m_createdBoxId;

			auto cmd_hidebox = new HideBoxInViewModel


			for(int i = 4; i --> 0;)
			{
				while(stack.canUndo()) stack.undo();
				while(stack.canRedo()) stack.redo();
			}

			delete constraint;
		}
};

QTEST_MAIN(HideBoxInViewModelTest)
#include "HideBoxInViewModelTest.moc"


