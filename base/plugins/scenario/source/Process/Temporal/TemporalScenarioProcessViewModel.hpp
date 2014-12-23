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
		using constraint_view_model_type = TemporalConstraintViewModel;
		// using event_type = TemporalEventViewModel;

		TemporalScenarioProcessViewModel(int id, ScenarioProcessSharedModel* model, QObject* parent);
		TemporalScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent);
		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override;
		virtual void deserialize(QDataStream&) override;

		virtual void createConstraintViewModel(int constraintModelId, int constraintViewModelId) override;

	public slots:
		virtual void on_constraintRemoved(int constraintId) override;
};
