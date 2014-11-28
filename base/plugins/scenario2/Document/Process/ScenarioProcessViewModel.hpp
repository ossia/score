#pragma once
#include <QNamedObject>
#include <interface/process/ProcessViewModelInterface.hpp>
class ScenarioProcessSharedModel;

class ScenarioProcessViewModel : public iscore::ProcessViewModelInterface
{
	Q_OBJECT

	public:
		ScenarioProcessViewModel(int id, int processId, QObject* parent);
		ScenarioProcessViewModel(QDataStream& s, QObject* parent);
		virtual ~ScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override { }
		virtual void deserialize(QDataStream&) override { }

	private:
		ScenarioProcessSharedModel* m_process;
};

