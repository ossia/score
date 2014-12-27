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

		template<typename Impl>
		TemporalScenarioProcessViewModel(Deserializer<Impl>& vis,
										 ScenarioProcessSharedModel* model,
										 QObject* parent):
			AbstractScenarioProcessViewModel{vis, model, parent}
		{
			vis.visit(*this);
		}

		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const override;

		virtual void makeConstraintViewModel(int constraintModelId,
											 int constraintViewModelId) override;

	public slots:
		virtual void on_constraintRemoved(int constraintId) override;

	protected:
		// TODO virtual void makeConstraintViewModel(QDataStream& s) override;
};
