#include "SpaceProcessFactory.hpp"
#include "SpaceProcess.hpp"
#include "SpaceLayerPresenter.hpp"
#include "SpaceLayerView.hpp"
QString SpaceProcessFactory::name() const
{
    return "Space";
}

Process *SpaceProcessFactory::makeModel(const TimeValue &duration, const id_type<Process> &id, QObject *parent)
{
    return new SpaceProcess{id, parent};
}

QByteArray SpaceProcessFactory::makeStaticLayerConstructionData() const
{
    return {};
}

Process *SpaceProcessFactory::loadModel(const VisitorVariant &, QObject *parent)
{
    return nullptr;
}

LayerPresenter *SpaceProcessFactory::makeLayerPresenter(const LayerModel & model, LayerView * view, QObject *parent)
{
    // TODO check with panel proxy
    return new SpaceLayerPresenter{model, view, parent};
}

LayerView *SpaceProcessFactory::makeLayerView(const LayerModel &layer, QGraphicsItem* parent)
{
    // TODO check with panel proxy
    return new SpaceLayerView{parent};
}
