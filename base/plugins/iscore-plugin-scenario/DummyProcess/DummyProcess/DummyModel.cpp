#include <iscore/serialization/VisitorCommon.hpp>
#include <algorithm>

#include "DummyLayerModel.hpp"
#include "DummyModel.hpp"
#include "DummyState.hpp"
#include <Process/Process.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>


namespace Dummy
{
DummyModel::DummyModel(
        const TimeValue& duration,
        const Id<ProcessModel>& id,
        QObject* parent):
    ProcessModel{duration, id, "DummyModel", parent}
{

}

DummyModel::DummyModel(
        const DummyModel& source,
        const Id<ProcessModel>& id,
        QObject* parent):
    ProcessModel{source.duration(), id, source.objectName(), parent}
{

}

DummyModel* DummyModel::clone(
        const Id<ProcessModel>& newId,
        QObject* newParent) const
{
    return new DummyModel{*this, newId, newParent};
}

ProcessFactoryKey DummyModel::concreteFactoryKey() const
{
    static const ProcessFactoryKey key{"7db45400-6033-425e-9ded-d60a35d4c4b2"};
    return key;
}

QString DummyModel::prettyName() const
{
    return "Dummy process";
}

QByteArray DummyModel::makeLayerConstructionData() const
{
    return {};
}

void DummyModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void DummyModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void DummyModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void DummyModel::startExecution()
{
}

void DummyModel::stopExecution()
{
}

void DummyModel::reset()
{
}

ProcessStateDataInterface* DummyModel::startStateData() const
{
    return &m_startState;
}

ProcessStateDataInterface* DummyModel::endStateData() const
{
    return &m_endState;
}

Selection DummyModel::selectableChildren() const
{
    return {};
}

Selection DummyModel::selectedChildren() const
{
    return {};
}

void DummyModel::setSelection(const Selection&) const
{
}

void DummyModel::serialize_impl(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

Process::LayerModel* DummyModel::makeLayer_impl(
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new DummyLayerModel{*this, viewModelId, parent};
}

Process::LayerModel* DummyModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new DummyLayerModel{
                        deserializer, *this, parent};

        return autom;
    });
}

Process::LayerModel* DummyModel::cloneLayer_impl(
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    return new DummyLayerModel{safe_cast<const DummyLayerModel&>(source), *this, newId, parent};
}
}
