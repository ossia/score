#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>

#include "Commands/Constraint/AddBoxToConstraint.hpp"

using namespace iscore;
using namespace Scenario::Command;

class AddBoxToConstraintTest: public QObject
{
		Q_OBJECT

	private slots:
		void CreateBoxTest()
		{
			ConstraintModel* constraint  = new ConstraintModel{id_type<ConstraintModel>{0},
															   id_type<AbstractConstraintViewModel>{0},
															   qApp};

			QCOMPARE((int)constraint->boxes().size(), 0);
			AddBoxToConstraint cmd(
			ObjectPath{ {"ConstraintModel", {}} });

			auto id = cmd.m_createdBoxId;

			cmd.redo();
			QCOMPARE((int)constraint->boxes().size(), 1);
			QCOMPARE(constraint->box(id)->parent(), constraint);

			cmd.undo();
			QCOMPARE((int)constraint->boxes().size(), 0);

			cmd.redo();
			QCOMPARE((int)constraint->boxes().size(), 1);
			QCOMPARE(constraint->box(id)->parent(), constraint);

			// Delete them else they stay in qApp !
			delete constraint;
		}


};

QTEST_MAIN(AddBoxToConstraintTest)
#include "AddBoxToConstraintTest.moc"

