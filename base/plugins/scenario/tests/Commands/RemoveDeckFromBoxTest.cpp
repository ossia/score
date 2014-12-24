#include <QtTest/QtTest>
#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"

using namespace iscore;
using namespace Scenario::Command;

class RemoveDeckFromBoxTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void test()
		{
		}
};

QTEST_MAIN(RemoveDeckFromBoxTest)
#include "RemoveDeckFromBoxTest.moc"


