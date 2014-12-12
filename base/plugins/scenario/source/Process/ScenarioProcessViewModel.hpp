#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/NamedObject.hpp>

#include <QMap>

class ScenarioProcessSharedModel;
class IntervalContentModel;
class IntervalModel;

class ScenarioProcessViewModel : public ProcessViewModelInterface
{
	Q_OBJECT

	public:
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
		void eventMoved(int eventId);
		void intervalMoved(int intervalId);


	private:
		IntervalModel* parentInterval() const;

		QMap<int, int> m_contentMapping; // Map between intervals and intervals contents.
};

