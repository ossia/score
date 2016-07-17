#include <Process/Dummy/DummyLayerModel.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "SimpleProcess.hpp"
#include "SimpleProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

SimpleProcessModel::SimpleProcessModel(
        const TimeValue& duration,
        const Id<ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, "SimpleProcessModel", parent},
    m_ossia_process{std::make_shared<SimpleProcess>()}
{
}

SimpleProcessModel::SimpleProcessModel(
        const SimpleProcessModel& source,
        const Id<ProcessModel>& id,
        QObject* parent):
    SimpleProcessModel{source.duration(), id, parent}
{
}

SimpleProcessModel::~SimpleProcessModel()
{
}

SimpleProcessModel* SimpleProcessModel::clone(
        const Id<ProcessModel>& newId,
        QObject* newParent) const
{
    return new SimpleProcessModel{*this, newId, newParent};
}

void SimpleProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::startExecution()
{
}

void SimpleProcessModel::stopExecution()
{
}

void SimpleProcessModel::reset()
{
}

ProcessStateDataInterface* SimpleProcessModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* SimpleProcessModel::endStateData() const
{
    return nullptr;
}

Selection SimpleProcessModel::selectableChildren() const
{
    return {};
}

Selection SimpleProcessModel::selectedChildren() const
{
    return {};
}

void SimpleProcessModel::setSelection(const Selection&) const
{
}

void SimpleProcessModel::serialize_impl(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const SimpleProcessModel& proc)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SimpleProcessModel& proc)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const SimpleProcessModel& proc)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(SimpleProcessModel& proc)
{
}

