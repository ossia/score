#include <QtTest/QtTest>
#include "Commands/Constraint/RemoveBoxFromConstraint.hpp"

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>

#include "Commands/Constraint/AddBoxToConstraint.hpp"

using namespace iscore;
using namespace Scenario::Command;

class RemoveBoxFromConstraintTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void test()
		{
			ConstraintModel* constraint  = new ConstraintModel{id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, qApp};

			AddBoxToConstraint cmd{
				ObjectPath{ {"ConstraintModel", {}} }};

			auto id = cmd.m_createdBoxId;
			cmd.redo();


			RemoveBoxFromConstraint cmd2{
				ObjectPath{ {"ConstraintModel", {}} },
				id};
			cmd2.redo();
			QCOMPARE((int)constraint->boxes().size(), 0);
			cmd2.undo();
			QCOMPARE((int)constraint->boxes().size(), 1);
			cmd.undo();
			QCOMPARE((int)constraint->boxes().size(), 0);
			cmd.redo();
			cmd2.redo();

			// Delete them else they stay in qApp !
			delete constraint;
		}
};

QTEST_MAIN(RemoveBoxFromConstraintTest)
#include "RemoveBoxFromConstraintTest.moc"


