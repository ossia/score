#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"
#include "Area/AreaParser.hpp"
#include "Space/SpaceModel.hpp"
#include "Area/Circle/CircleAreaModel.hpp"
#include "Area/Pointer/PointerAreaModel.hpp"
#include <src/Executor/ProcessExecutor.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Space
{
ProcessModel::ProcessModel(
        const iscore::DocumentContext& doc,
        const TimeValue &duration,
        const Id<Process::ProcessModel> &id,
        QObject *parent):
    Process::ProcessModel{id, ProcessMetadata::processObjectName(), parent},
    m_space{new SpaceModel{
            Id<SpaceModel>(0),
            this}},
    m_context{doc, *m_space, doc.plugin<DeviceDocumentPlugin>()}
{
    metadata.setName(QString("Space.%1").arg(*this->id().val()));
    using namespace GiNaC;
    using namespace spacelib;

    auto x_dim = new DimensionModel{"x", Id<DimensionModel>{0}, m_space};
    auto y_dim = new DimensionModel{"y", Id<DimensionModel>{1}, m_space};

    m_space->addDimension(x_dim);
    m_space->addDimension(y_dim);

    auto vp = new ViewportModel{Id<ViewportModel>{0}, m_space};
    m_space->addViewport(vp);

    setDuration(duration);
}

ProcessModel *ProcessModel::clone(
        const Id<Process::ProcessModel> &newId,
        QObject *newParent) const
{
    auto& doc = iscore::IDocument::documentContext(*newParent);
    return new ProcessModel{doc, this->duration(), newId, newParent};
}

const ProcessFactoryKey& ProcessModel::key() const
{
    return ProcessMetadata::factoryKey();
}

QString ProcessModel::prettyName() const
{
    return tr("Space process");
}

void ProcessModel::setDurationAndScale(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::setDurationAndGrow(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::setDurationAndShrink(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::reset()
{
    ISCORE_TODO;
}

ProcessStateDataInterface* ProcessModel::startStateData() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface *ProcessModel::endStateData() const
{
    ISCORE_TODO;
    return nullptr;
}

Selection ProcessModel::selectableChildren() const
{
    ISCORE_TODO;
    return {};
}

Selection ProcessModel::selectedChildren() const
{
    ISCORE_TODO;
    return {};
}

void ProcessModel::setSelection(const Selection &s) const
{
    ISCORE_TODO;
}

void ProcessModel::serialize(const VisitorVariant &vis) const
{
    ISCORE_TODO;
}


Process::LayerModel *ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel> &viewModelId,
        const QByteArray &constructionData,
        QObject *parent)
{
    return new LayerModel{viewModelId, *this, parent};
}

Process::LayerModel *ProcessModel::loadLayer_impl(
        const VisitorVariant &,
        QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}

Process::LayerModel *ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel> &newId,
        const Process::LayerModel &source,
        QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}


void ProcessModel::startExecution()
{
}

void ProcessModel::stopExecution()
{
    // Reset everything to the default values.
}
}
