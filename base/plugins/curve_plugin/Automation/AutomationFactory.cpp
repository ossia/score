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

ProcessView* AutomationFactory::makeView(
        const ProcessViewModel& view,
        QObject* parent)
{
    return new AutomationView {static_cast<QGraphicsObject*>(parent) };
}


ProcessPresenter* AutomationFactory::makePresenter(
        const ProcessViewModel& viewModel,
        ProcessView* view,
        QObject* parent)
{
    return new AutomationPresenter {viewModel, view, parent};
}
