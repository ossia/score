#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>
#include "ProcessInterface/ProcessList.hpp"

#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/RemoveProcessFromConstraint.hpp"
#include <Process/ScenarioProcessFactory.hpp>

using namespace iscore;
using namespace Scenario::Command;


class AddProcessToConstraintTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void CreateCommandTest()
		{
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			ConstraintModel* int_model  = new ConstraintModel{id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, qApp};

			int_model->createBox(id_type<BoxModel>{421}); // TODO use command instead.
			AddProcessToConstraint cmd(
			{
				{"ConstraintModel", {}}
			}, "Scenario");
			cmd.redo();
			cmd.undo();
			cmd.redo();

			// Delete them else they stay in qApp !
			delete int_model;
			delete obj;
		}

};

QTEST_MAIN(AddProcessToConstraintTest)
#include "AddProcessToConstraintTest.moc"

