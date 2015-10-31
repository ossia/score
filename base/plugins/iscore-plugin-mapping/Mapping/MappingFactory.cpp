#include "MappingFactory.hpp"
#include "MappingModel.hpp"
#include "MappingView.hpp"
#include "MappingPresenter.hpp"


Process* MappingFactory::makeModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent)
{
    return new MappingModel{duration, id, parent};
}

LayerView* MappingFactory::makeLayerView(
        const LayerModel&,
        QGraphicsItem* parent)
{
    return new MappingView{parent};
}


LayerPresenter* MappingFactory::makeLayerPresenter(
        const LayerModel& viewModel,
        LayerView* view,
        QObject* parent)
{
    return new MappingPresenter {
        safe_cast<const MappingLayerModel&>(viewModel),
                safe_cast<MappingView*>(view),
                parent};
}


QByteArray MappingFactory::makeStaticLayerConstructionData() const
{
    return {};
}
