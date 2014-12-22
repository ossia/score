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
		using model_type = ScenarioProcessSharedModel;

		ScenarioProcessViewModel(int id, ScenarioProcessSharedModel* model, QObject* parent);
		ScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent);
		virtual ~ScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override { }
		virtual void deserialize(QDataStream&) override { }

	signals: // Transmitted from ScenarioProcessSharedModel. They might become slots.
		void eventCreated(int eventId);
		void constraintCreated(int constraintId);
		void eventDeleted(int eventId);
		void constraintDeleted(int constraintId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);


	private:
		QMap<int, int> m_contentMapping; // Map between constraints and constraints contents.
};

