#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"

SpaceProcess::SpaceProcess(const id_type<Process> &id, QObject *parent):
    Process{id, "SpaceProcessModel", parent}
{

}

Process *SpaceProcess::clone(const id_type<Process> &newId, QObject *newParent) const
{
    return new SpaceProcess{newId, newParent};
}

QString SpaceProcess::processName() const
{
    return "Space";
}

void SpaceProcess::setDurationAndScale(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::setDurationAndGrow(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::setDurationAndShrink(const TimeValue &newDuration)
{
    ISCORE_TODO;
}

void SpaceProcess::reset()
{
    ISCORE_TODO;
}

ProcessStateDataInterface *SpaceProcess::startState() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface *SpaceProcess::endState() const
{
    ISCORE_TODO;
    return nullptr;
}

Selection SpaceProcess::selectableChildren() const
{
    ISCORE_TODO;
    return {};
}

Selection SpaceProcess::selectedChildren() const
{
    ISCORE_TODO;
    return {};
}

void SpaceProcess::setSelection(const Selection &s) const
{
    ISCORE_TODO;
}

void SpaceProcess::serialize(const VisitorVariant &vis) const
{
    ISCORE_TODO;
}

LayerModel *SpaceProcess::makeLayer_impl(const id_type<LayerModel> &viewModelId, const QByteArray &constructionData, QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}

LayerModel *SpaceProcess::loadLayer_impl(const VisitorVariant &, QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}

LayerModel *SpaceProcess::cloneLayer_impl(const id_type<LayerModel> &newId, const LayerModel &source, QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}
