#include "DummyModel.hpp"
#include "DummyLayerModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>


DummyModel::DummyModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent):
    Process{duration, id, processName(), parent}
{

}

DummyModel::DummyModel(
        const DummyModel& source,
        const Id<Process>& id,
        QObject* parent):
    Process{source.duration(), id, processName(), parent}
{

}

DummyModel* DummyModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new DummyModel{*this, newId, newParent};
}

QString DummyModel::processName() const
{
    return "Dummy";
}

QString DummyModel::userFriendlyDescription() const
{
    return "Dummy process";
}

QByteArray DummyModel::makeLayerConstructionData() const
{
    return {};
}

void DummyModel::setDurationAndScale(const TimeValue& newDuration)
{
    (void)newDuration;
}

void DummyModel::setDurationAndGrow(const TimeValue& newDuration)
{
    (void)newDuration;
}

void DummyModel::setDurationAndShrink(const TimeValue& newDuration)
{
    (void)newDuration;
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

ProcessStateDataInterface* DummyModel::startState() const
{
    return nullptr;
}

ProcessStateDataInterface* DummyModel::endState() const
{
    return nullptr;
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
