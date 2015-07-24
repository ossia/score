#include "AutomationFactory.hpp"
#include "AutomationModel.hpp"
#include "AutomationView.hpp"
#include "AutomationPresenter.hpp"


Process* AutomationFactory::makeModel(
        const TimeValue& duration,
        const id_type<Process>& id,
        QObject* parent)
{
    return new AutomationModel {duration, id, parent};
}

LayerView* AutomationFactory::makeLayerView(
        const LayerModel& view,
        QObject* parent)
{
    return new AutomationView {static_cast<QGraphicsObject*>(parent) };
}


LayerPresenter* AutomationFactory::makeLayerPresenter(
        const LayerModel& viewModel,
        LayerView* view,
        QObject* parent)
{
    return new AutomationPresenter {viewModel, view, parent};
}


QByteArray AutomationFactory::makeStaticLayerConstructionData() const
{
    return {};
}
