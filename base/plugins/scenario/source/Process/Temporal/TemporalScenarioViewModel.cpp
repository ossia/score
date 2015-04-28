#include "TemporalScenarioViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>

TemporalScenarioViewModel::TemporalScenarioViewModel(id_type<ProcessViewModelInterface> viewModelId,
        ScenarioModel* model,
        QObject* parent) :
    AbstractScenarioViewModel {viewModelId,
                              "TemporalScenarioViewModel",
                              model,
                              parent
}
{
}

TemporalScenarioViewModel::TemporalScenarioViewModel(const TemporalScenarioViewModel* source,
        id_type<ProcessViewModelInterface> id,
        ScenarioModel* newScenario,
        QObject* parent) :
    AbstractScenarioViewModel {source,
                              id,
                              "TemporalScenarioViewModel",
                              newScenario,
                              parent
}
{
    for(TemporalConstraintViewModel* src_constraint : constraintsViewModels(*source))
    {
        // TODO some room for optimization here
        addConstraintViewModel(
                    src_constraint->clone(
                        src_constraint->id(),
                        newScenario->constraint(src_constraint->model()->id()),
                        this));
    }
}

class TemporalScenarioPanelProxy : public ProcessViewModelPanelProxy
{
    public:
        TemporalScenarioPanelProxy(TemporalScenarioViewModel* parent):
            ProcessViewModelPanelProxy{parent}
        {

        }

        ProcessViewModelInterface* viewModel() override
        {
            return static_cast<TemporalScenarioViewModel*>(parent());
        }
};

ProcessViewModelPanelProxy*TemporalScenarioViewModel::make_panelProxy()
{
    return new TemporalScenarioPanelProxy{this};
}


void TemporalScenarioViewModel::makeConstraintViewModel(id_type<ConstraintModel> constraintModelId,
                                                        id_type<AbstractConstraintViewModel> constraintViewModelId)
{
    auto constraint_model = model(this)->constraint(constraintModelId);

    auto constraint_view_model =
        constraint_model->makeConstraintViewModel<constraint_view_model_type> (
            constraintViewModelId,
            this);

    addConstraintViewModel(constraint_view_model);
}

void TemporalScenarioViewModel::addConstraintViewModel(constraint_view_model_type* constraint_view_model)
{
    m_constraints.push_back(constraint_view_model);

    emit constraintViewModelCreated(constraint_view_model->id());
}

void TemporalScenarioViewModel::on_constraintRemoved(id_type<ConstraintModel> constraintSharedModelId)
{
    for(auto& constraint_view_model : constraintsViewModels(*this))
    {
        if(constraint_view_model->model()->id() == constraintSharedModelId)
        {
            removeConstraintViewModel(constraint_view_model->id());
            return;
        }
    }
}
