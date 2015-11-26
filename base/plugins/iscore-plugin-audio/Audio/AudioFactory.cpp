#include "AudioFactory.hpp"

QString Audio::ProcessFactory::prettyName() const
{ // In factory list
    return Audio::ProcessMetadata::factoryPrettyName();
}

const ProcessFactoryKey&Audio::ProcessFactory::key_impl() const
{
    return Audio::ProcessMetadata::factoryKey();
}

::Process*Audio::ProcessFactory::makeModel(const TimeValue& duration, const Id<::Process>& id, QObject* parent)
{
    return new Audio::ProcessModel{duration, id, parent};
}

QByteArray Audio::ProcessFactory::makeStaticLayerConstructionData() const
{
    return {};
}

::Process*Audio::ProcessFactory::loadModel(const VisitorVariant& vis, QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new Audio::ProcessModel{deserializer, parent};});
}

LayerPresenter*Audio::ProcessFactory::makeLayerPresenter(const LayerModel& model, LayerView* v, QObject* parent)
{
    return new DummyLayerPresenter{model, dynamic_cast<DummyLayerView*>(v), parent};
}

LayerView*Audio::ProcessFactory::makeLayerView(const LayerModel&, QGraphicsItem* parent)
{
    return new DummyLayerView{parent};
}
