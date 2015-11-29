#include <iscore/serialization/VisitorCommon.hpp>
#include <algorithm>

#include "DummyLayerModel.hpp"
#include "DummyModel.hpp"
#include "DummyState.hpp"
#include <Process/Process.hpp>

class LayerModel;
class ProcessStateDataInterface;
class QObject;
template <typename tag, typename impl> class id_base_t;


DummyModel::DummyModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent):
    Process{duration, id, "DummyModel", parent}
{

}

DummyModel::DummyModel(
        const DummyModel& source,
        const Id<Process>& id,
        QObject* parent):
    Process{source.duration(), id, source.objectName(), parent}
{

}

DummyModel* DummyModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new DummyModel{*this, newId, newParent};
}

const ProcessFactoryKey& DummyModel::key() const
{
    static const ProcessFactoryKey key{"Dummy"};
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

void DummyModel::serialize(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

LayerModel* DummyModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new DummyLayerModel{*this, viewModelId, parent};
}

LayerModel* DummyModel::loadLayer_impl(
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

LayerModel* DummyModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new DummyLayerModel{safe_cast<const DummyLayerModel&>(source), *this, newId, parent};
}
