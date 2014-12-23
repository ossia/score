#pragma once

#include <QMap>
#include "Process/AbstractScenarioProcessViewModel.hpp"
class ScenarioProcessSharedModel;
class BoxModel;
class TemporalConstraintViewModel;


class ConstraintModel;


class TemporalScenarioProcessViewModel : public AbstractScenarioProcessViewModel
{
	Q_OBJECT

	public:
		using model_type = ScenarioProcessSharedModel;
		using constraint_type = TemporalConstraintViewModel;
		// using event_type = TemporalEventViewModel;

		TemporalScenarioProcessViewModel(int id, ScenarioProcessSharedModel* model, QObject* parent);
		TemporalScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent);
		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override;
		virtual void deserialize(QDataStream&) override;

		virtual void createConstraintViewModel(int constraintModelId, int constraintViewModelId) override;

		//QPair<bool, int>& boxDisplayedForConstraint(int constraintId);


	signals:

		// To transform in order to use view models instead
		void eventCreated(int eventId);
		void eventDeleted(int eventId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);

	public slots:
		void on_constraintCreated(int constraintId);
		void on_constraintRemoved(int constraintId);




		// TO REMOVE.
		// Map between constraints and constraints contents.
		// If the QPair bool is false, there is no box displayed, and the int content is undefined.
		// Else, the int is the id of the box to be displayed in this view model.
		// TODO why not SettableIdentifier instead?
		//QMap<int, QPair<bool, int>> m_constraintToBox;
};
