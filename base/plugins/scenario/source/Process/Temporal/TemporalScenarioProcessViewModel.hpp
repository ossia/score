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
	friend QDataStream& operator >>(QDataStream& s, TemporalScenarioProcessViewModel& pvm);

	public:
		using model_type = ScenarioProcessSharedModel;
		using constraint_view_model_type = TemporalConstraintViewModel;
		// using event_type = TemporalEventViewModel;

		TemporalScenarioProcessViewModel(int id,
										 ScenarioProcessSharedModel* model,
										 QObject* parent);
		TemporalScenarioProcessViewModel(QDataStream& s,
										 ScenarioProcessSharedModel* model,
										 QObject* parent);
		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(QDataStream&) const override;

		virtual void makeConstraintViewModel(int constraintModelId,
											 int constraintViewModelId) override;

	public slots:
		virtual void on_constraintRemoved(int constraintId) override;

	protected:
		virtual void makeConstraintViewModel(QDataStream& s) override;
};
