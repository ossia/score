#include "SpaceProcessFactory.hpp"
#include "SpaceProcess.hpp"
#include "SpaceLayerPresenter.hpp"
#include "SpaceLayerView.hpp"
const ProcessFactoryKey& SpaceProcessFactory::key_impl() const
{
    return SpaceProcessMetadata::factoryKey();
}

QString SpaceProcessFactory::prettyName() const
{
    return "Space Process";
}

Process *SpaceProcessFactory::makeModel(const TimeValue &duration, const Id<Process> &id, QObject *parent)
{
    return new SpaceProcess{duration, id, parent};
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
