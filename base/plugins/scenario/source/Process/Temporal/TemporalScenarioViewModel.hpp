#pragma once

#include <QMap>
#include "source/Process/AbstractScenarioViewModel.hpp"
class ScenarioModel;
class BoxModel;
class TemporalConstraintViewModel;


class ConstraintModel;


class TemporalScenarioViewModel : public AbstractScenarioViewModel
{
        Q_OBJECT
        friend QDataStream& operator >> (QDataStream& s, TemporalScenarioViewModel& pvm);

    public:
        using model_type = ScenarioModel;
        using constraint_view_model_type = TemporalConstraintViewModel;
        // using event_type = TemporalEventViewModel;

        TemporalScenarioViewModel (id_type<ProcessViewModelInterface> id,
                                   ScenarioModel* model,
                                   QObject* parent);

        TemporalScenarioViewModel (const TemporalScenarioViewModel* source,
                                   id_type<ProcessViewModelInterface> id,
                                   ScenarioModel* model,
                                   QObject* parent);

        template<typename Impl>
        TemporalScenarioViewModel (Deserializer<Impl>& vis,
                                   ScenarioModel* model,
                                   QObject* parent) :
            AbstractScenarioViewModel {vis, model, parent}
        {
            vis.writeTo (*this);
        }

        virtual ~TemporalScenarioViewModel() = default;

        virtual void serialize (SerializationIdentifier identifier,
                                void* data) const override;

        virtual void makeConstraintViewModel (id_type<ConstraintModel> constraintModelId,
                                              id_type<AbstractConstraintViewModel> constraintViewModelId) override;

        void addConstraintViewModel (constraint_view_model_type* constraint_view_model);

    public slots:
        virtual void on_constraintRemoved (id_type<ConstraintModel> constraintId) override;

};
