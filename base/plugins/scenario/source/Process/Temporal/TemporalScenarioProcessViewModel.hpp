#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/NamedObject.hpp>

#include <QMap>

class ScenarioProcessSharedModel;
class BoxModel;
class ConstraintModel;


class TemporalScenarioProcessViewModel : public ProcessViewModelInterface
{
	Q_OBJECT

	public:
		using model_type = ScenarioProcessSharedModel;

		TemporalScenarioProcessViewModel(int id, ScenarioProcessSharedModel* model, QObject* parent);
		TemporalScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent);
		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override;
		virtual void deserialize(QDataStream&) override;

		QPair<bool, int>& boxDisplayedForConstraint(int constraintId);

	signals: // Forwarded from ScenarioProcessSharedModel.
		void eventCreated(int eventId);
		void constraintCreated(int constraintId);
		void eventDeleted(int eventId);
		void constraintRemoved(int constraintId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);

	public slots:
		void on_constraintCreated(int constraintId);
		void on_constraintRemoved(int constraintId);

	private:
		// Map between constraints and constraints contents.
		// If the QPair bool is false, there is no box displayed, and the int content is undefined.
		// Else, the int is the id of the box to be displayed in this view model.
		// TODO why not SettableIdentifier instead?
		QMap<int, QPair<bool, int>> m_constraintToBox;
};

