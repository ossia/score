#include "AutomationFactory.hpp"
#include "AutomationModel.hpp"
#include "AutomationView.hpp"
#include "AutomationPresenter.hpp"


ProcessModel* AutomationFactory::makeModel(
        const TimeValue& duration,
        const id_type<ProcessModel>& id,
        QObject* parent)
{
    return new AutomationModel {duration, id, parent};
}

ProcessViewInterface* AutomationFactory::makeView(
        const ProcessViewModelInterface& view,
        QObject* parent)
{
    return new AutomationView {static_cast<QGraphicsObject*>(parent) };
}


ProcessPresenterInterface* AutomationFactory::makePresenter(
        const ProcessViewModelInterface& viewModel,
        ProcessViewInterface* view,
        QObject* parent)
{
    return new AutomationPresenter {viewModel, view, parent};
}
