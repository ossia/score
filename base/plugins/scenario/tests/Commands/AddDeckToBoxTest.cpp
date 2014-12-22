#include <QtTest/QtTest>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>

#include "Commands/Constraint/Box/AddDeckToBox.hpp"

using namespace iscore;
using namespace Scenario::Command;

class AddDeckToBoxTest: public QObject
{
		Q_OBJECT

	private slots:
		void CreateDeckTest()
		{
			BoxModel* box  = new BoxModel{0, qApp};

			QCOMPARE((int)box->decks().size(), 0);
			AddDeckToBox cmd(
			ObjectPath{ {"BoxModel", {}} });
			auto id = cmd.m_createdDeckId;

			cmd.redo();
			QCOMPARE((int)box->decks().size(), 1);
			QCOMPARE(box->deck(id)->parent(), box);

			cmd.undo();
			QCOMPARE((int)box->decks().size(), 0);

			cmd.redo();
			QCOMPARE((int)box->decks().size(), 1);
			QCOMPARE(box->deck(id)->parent(), box);

			// Delete them else they stay in qApp !
			delete box;
		}


};

QTEST_MAIN(AddDeckToBoxTest)
#include "AddDeckToBoxTest.moc"

