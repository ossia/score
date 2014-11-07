#include <QtTest/QtTest>
#include <core/presenter/command/Command.hpp>
#include <core/presenter/command/CommandQueue.hpp>
#include <core/QNamedObject>
using namespace iscore;


class FakeParent : public QObject
{

} m_globalParent;

struct FakeModel : public QNamedObject
{
	FakeModel(QObject* parent): QNamedObject{parent, "FakeModel"}
	{
	}

	int modelId{};
	int value{};
};

class FakeCommand : public Command
{
	public:
		FakeCommand(int modelId):
			Command{"", "FakeCommand", "" },
			m_modelId{modelId}
		{

		}

		virtual void undo()
		{
			auto children = m_globalParent.findChildren<FakeModel*>("FakeModel");
			for(auto& model : children)
			{
				if(model->modelId == m_modelId)
				{
					model->value--;
				}
			}
		}

		virtual void redo()
		{
			auto children = m_globalParent.findChildren<FakeModel*>("FakeModel");
			for(auto& model : children)
			{
				if(model->modelId == m_modelId)
				{
					model->value++;
				}
			}
		}

		virtual int id() const
		{
			return 1;
		}

		virtual QByteArray serialize()
		{
		}

		virtual void deserialize(QByteArray)
		{
		}

	private:
		int m_modelId{};
};

class TestCommand: public QObject
{
		Q_OBJECT
	public:
		TestCommand() : QObject{}
		{
			m_model = new FakeModel{&m_globalParent};
		}

	private slots:
		void CommandTest()
		{
			QVERIFY(m_model->value == 0);
			auto cmd = new FakeCommand{0};
			m_commandQueue.push(cmd);
			QVERIFY(m_model->value == 1);
			m_commandQueue.undo();
			QVERIFY(m_model->value == 0);
			m_commandQueue.redo();
			QVERIFY(m_model->value == 1);
		}

	private:
		CommandQueue m_commandQueue;
		FakeModel* m_model{};
};

QTEST_MAIN(TestCommand)
#include "test_command.moc"

