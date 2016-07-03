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

UuidKey<Process::ProcessFactory>DummyModel::concreteFactoryKey() const
{
    static const UuidKey<Process::ProcessFactory>key{"7db45400-6033-425e-9ded-d60a35d4c4b2"};
    return key;
}

QString DummyModel::prettyName() const
{
    return "Dummy process";
}

ProcessStateDataInterface* DummyModel::startStateData() const
{
    return &m_startState;
}

ProcessStateDataInterface* DummyModel::endStateData() const
{
    return &m_endState;
}

void DummyModel::serialize_impl(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

}
