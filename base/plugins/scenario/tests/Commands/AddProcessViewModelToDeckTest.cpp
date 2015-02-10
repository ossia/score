#include <QtTest/QtTest>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include <Process/ScenarioProcessFactory.hpp>
#include "ProcessInterface/ProcessList.hpp"

#include <QUndoStack>

using namespace iscore;
using namespace Scenario::Command;

class AddProcessViewModelToDeckTest: public QObject
{
		Q_OBJECT

	private slots:
		void CreateViewModelTest()
		{
			QUndoStack stack;
			// Maybe do a fake process list, with a fake process for unit tests.
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			// Setup
			ConstraintModel* constraint  = new ConstraintModel{id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, qApp};

			auto cmd_proc = new AddProcessToConstraint (
			{
				{"ConstraintModel", {}}
			}, "Scenario");
			stack.push(cmd_proc);
			auto procId = cmd_proc->m_createdProcessId;

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
			 {"DeckModel", deckId}},
			{{"ConstraintModel", {}},
			 {"ScenarioProcessSharedModel", procId}});
			stack.push(cmd_pvm);

			for(int i = 4; i --> 0;)
			{
				while(stack.canUndo()) stack.undo();
				while(stack.canRedo()) stack.redo();
			}

			delete constraint;
		}
};

QTEST_MAIN(AddProcessViewModelToDeckTest)
#include "AddProcessViewModelToDeckTest.moc"

