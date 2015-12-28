#include "SpaceProcessFactory.hpp"
#include "SpaceProcess.hpp"
#include "SpaceLayerPresenter.hpp"
#include "SpaceLayerView.hpp"

namespace Space
{
const ProcessFactoryKey& ProcessFactory::key_impl() const
{
    return ProcessMetadata::factoryKey();
}

QString ProcessFactory::prettyName() const
{
    return "Space Process";
}

::Process *ProcessFactory::makeModel(
        const TimeValue &duration,
        const Id<::Process> &id,
        QObject *parent)
{
    auto& doc = iscore::IDocument::documentContext(*parent);
    return new ProcessModel{doc, duration, id, parent};
}

QByteArray ProcessFactory::makeStaticLayerConstructionData() const
{
    return {};
}

::Process *ProcessFactory::loadModel(const VisitorVariant &, QObject *parent)
{
    return nullptr;
}

LayerPresenter *ProcessFactory::makeLayerPresenter(const ::LayerModel & model, LayerView * view, QObject *parent)
{
    // TODO check with panel proxy
    return new SpaceLayerPresenter{model, view, parent};
}

LayerView *ProcessFactory::makeLayerView(const ::LayerModel &layer, QGraphicsItem* parent)
{
    // TODO check with panel proxy
    return new SpaceLayerView{parent};
}
}
