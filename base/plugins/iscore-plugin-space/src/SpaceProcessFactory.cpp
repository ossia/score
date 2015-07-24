#include "SpaceProcessFactory.hpp"

QString SpaceProcessFactory::name() const
{
    return "SpaceProcess";
}

ProcessModel *SpaceProcessFactory::makeModel(const TimeValue &duration, const id_type<ProcessModel> &id, QObject *parent)
{
    return nullptr;
}

QByteArray SpaceProcessFactory::makeStaticLayerConstructionData() const
{
    return {};
}

ProcessModel *SpaceProcessFactory::loadModel(const VisitorVariant &, QObject *parent)
{
    return nullptr;
}

ProcessPresenter *SpaceProcessFactory::makePresenter(const LayerModel &, Layer *, QObject *parent)
{
    return nullptr;
}

Layer *SpaceProcessFactory::makeView(const LayerModel &view, QObject *parent)
{
    return nullptr;
}
