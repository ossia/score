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

Process::ProcessModel *ProcessFactory::makeModel(
        const TimeValue &duration,
        const Id<Process::ProcessModel> &id,
        QObject *parent)
{
    auto& doc = iscore::IDocument::documentContext(*parent);
    return new ProcessModel{doc, duration, id, parent};
}

QByteArray ProcessFactory::makeStaticLayerConstructionData() const
{
    return {};
}

Process::ProcessModel *ProcessFactory::loadModel(
        const VisitorVariant &,
        QObject *parent)
{
    return nullptr;
}

Process::LayerPresenter *ProcessFactory::makeLayerPresenter(
        const Process::LayerModel & model,
        Process::LayerView * view,
        QObject *parent)
{
    // TODO check with panel proxy
    return new LayerPresenter{model, view, parent};
}

Process::LayerView *ProcessFactory::makeLayerView(
        const Process::LayerModel &layer,
        QGraphicsItem* parent)
{
    // TODO check with panel proxy
    return new LayerView{parent};
}
}
