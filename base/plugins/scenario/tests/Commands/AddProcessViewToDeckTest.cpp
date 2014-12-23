#include <QtTest/QtTest>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>

#include "Commands/Constraint/Box/Deck/AddProcessViewToDeck.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include <Process/ScenarioProcessFactory.hpp>
#include "Control/ProcessList.hpp"

using namespace iscore;
using namespace Scenario::Command;

class AddProcessViewModelToDeckTest: public QObject
{
		Q_OBJECT

	private slots:
		void CreateViewModelTest()
		{
			// Maybe do a fake process list, with a fake process for unit tests.
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			// Setup
			ConstraintModel* constraint  = new ConstraintModel{0, qApp};

			AddProcessToConstraintCommand cmd_proc(
			{
				{"ConstraintModel", {}}
			}, "Scenario");
			cmd_proc.redo();
			auto procId = cmd_proc.m_createdProcessId;

			AddBoxToConstraint cmd_box(
			ObjectPath{ {"ConstraintModel", {}} });
			cmd_box.redo();
			auto boxId = cmd_box.m_createdBoxId;

			AddDeckToBox cmd_deck(
			ObjectPath{
				{"ConstraintModel", {}},
				{"BoxModel", boxId}});
			auto deckId = cmd_deck.m_createdDeckId;
			cmd_deck.redo();

			AddProcessViewModelToDeck cmd_pvm(
			{{"ConstraintModel", {}},
			 {"BoxModel", boxId},
			 {"DeckModel", deckId}}, procId);
			cmd_pvm.redo();

			delete constraint;
		}


};

QTEST_MAIN(AddProcessViewModelToDeckTest)
#include "AddProcessViewToDeckTest.moc"

