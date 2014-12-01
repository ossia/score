#pragma once
#include <QNamedObject>
#include <QMap>
#include <interface/process/ProcessViewModelInterface.hpp>
class ScenarioProcessSharedModel;
class IntervalContentModel;
class IntervalModel;

class ScenarioProcessViewModel : public iscore::ProcessViewModelInterface
{
	Q_OBJECT

	public:
		// TODO It's not the process id, but the process path that should be passed.
		ScenarioProcessViewModel(int id, int processId, QObject* parent);
		ScenarioProcessViewModel(QDataStream& s, QObject* parent);
		virtual ~ScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override { }
		virtual void deserialize(QDataStream&) override { }

		ScenarioProcessSharedModel* model();
		IntervalContentModel* intervals();

	signals: // Transmitted from ScenarioProcessSharedModel. They might become slots.
		void eventCreated(int eventId);
		void intervalCreated(int intervalId);
		void eventDeleted(int eventId);
		void intervalDeleted(int intervalId);


	private:
		IntervalModel* parentInterval() const;

		QMap<int, int> m_contentMapping;
		// TODO a map between interval id's and interval content model id's.
};

