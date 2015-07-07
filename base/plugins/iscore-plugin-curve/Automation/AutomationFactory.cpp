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

Layer* AutomationFactory::makeView(
        const LayerModel& view,
        QObject* parent)
{
    return new AutomationView {static_cast<QGraphicsObject*>(parent) };
}


ProcessPresenter* AutomationFactory::makePresenter(
        const LayerModel& viewModel,
        Layer* view,
        QObject* parent)
{
    return new AutomationPresenter {viewModel, view, parent};
}


QByteArray AutomationFactory::makeStaticLayerConstructionData() const
{
    return {};
}
