#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/NamedObject.hpp>

#include <QMap>

class ScenarioProcessSharedModel;
class BoxModel;
class ConstraintModel;

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
		BoxModel* constraints();

	signals: // Transmitted from ScenarioProcessSharedModel. They might become slots.
		void eventCreated(int eventId);
		void constraintCreated(int constraintId);
		void eventDeleted(int eventId);
		void constraintDeleted(int constraintId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);


	private:
		ConstraintModel* parentConstraint() const;

		QMap<int, int> m_contentMapping; // Map between constraints and constraints contents.
};

