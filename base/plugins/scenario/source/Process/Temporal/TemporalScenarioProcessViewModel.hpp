#pragma once

#include <QMap>
#include "source/Process/AbstractScenarioProcessViewModel.hpp"
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

		TemporalScenarioProcessViewModel(id_type<ProcessViewModelInterface> id,
										 ScenarioProcessSharedModel* model,
										 QObject* parent);

		template<typename Impl>
		TemporalScenarioProcessViewModel(Deserializer<Impl>& vis,
										 ScenarioProcessSharedModel* model,
										 QObject* parent):
			AbstractScenarioProcessViewModel{vis, model, parent}
		{
			vis.writeTo(*this);
		}

		virtual ~TemporalScenarioProcessViewModel() = default;

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const override;

		virtual void makeConstraintViewModel(id_type<ConstraintModel> constraintModelId,
											 id_type<AbstractConstraintViewModel> constraintViewModelId) override;

		void addConstraintViewModel(constraint_view_model_type* constraint_view_model);

	public slots:
		virtual void on_constraintRemoved(id_type<ConstraintModel> constraintId) override;

};
