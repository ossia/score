#include "AutomationFactory.hpp"
#include "AutomationModel.hpp"
#include "AutomationView.hpp"
#include "AutomationPresenter.hpp"


ProcessSharedModelInterface* AutomationFactory::makeModel(TimeValue duration, id_type<ProcessSharedModelInterface> id,
        QObject* parent)
{
    return new AutomationModel {duration, id, parent};
}

ProcessViewInterface* AutomationFactory::makeView(ProcessViewModelInterface* view, QObject* parent)
{
    return new AutomationView {static_cast<QGraphicsObject*>(parent) };
}


ProcessPresenterInterface* AutomationFactory::makePresenter(ProcessViewModelInterface* viewModel,
        ProcessViewInterface* view,
        QObject* parent)
{
    return new AutomationPresenter {viewModel, view, parent};
}
