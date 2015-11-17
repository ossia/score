#include "LoopProcessModel.hpp"
#include <Loop/LoopLayer.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>

LoopProcessModel::LoopProcessModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent):
    Process{duration, id, LoopProcessMetadata::processObjectName(), parent},
    BaseScenarioContainer{this}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      iscore::IDocument::documentFromObject(parent),
                      this};

    BaseScenarioContainer::init();

    baseConstraint().duration.setRigid(false);
    ConstraintDurations::Algorithms::changeAllDurations(baseConstraint(), duration);
    endEvent().setDate(duration);
    endTimeNode().setDate(duration);

    baseConstraint().setHeightPercentage(0.05);
    BaseScenarioContainer::startState().setHeightPercentage(0.05);
    BaseScenarioContainer::endState().setHeightPercentage(0.05);
    BaseScenarioContainer::startEvent().setExtent({0.02, 0.2});
    BaseScenarioContainer::endEvent().setExtent({0.02, 0.2});
    BaseScenarioContainer::startTimeNode().setExtent({0, 1});
    BaseScenarioContainer::endTimeNode().setExtent({0, 1});
}

LoopProcessModel::LoopProcessModel(
        const LoopProcessModel& source,
        const Id<Process>& id,
        QObject* parent):
    Process{source.duration(), id, LoopProcessMetadata::processObjectName(), parent},
    BaseScenarioContainer{this}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
                      this};
}

LoopProcessModel* LoopProcessModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new LoopProcessModel{*this, newId, newParent};
}

QString LoopProcessModel::prettyName() const
{
    return "Loop Process";
}

QByteArray LoopProcessModel::makeLayerConstructionData() const
{
    return {};
}

void LoopProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void LoopProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void LoopProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void LoopProcessModel::startExecution()
{
}

void LoopProcessModel::stopExecution()
{
}

void LoopProcessModel::reset()
{
}

ProcessStateDataInterface* LoopProcessModel::startState() const
{
    return nullptr;
}

ProcessStateDataInterface* LoopProcessModel::endState() const
{
    return nullptr;
}

Selection LoopProcessModel::selectableChildren() const
{
    return {};
}

Selection LoopProcessModel::selectedChildren() const
{
    return {};
}

void LoopProcessModel::setSelection(const Selection&) const
{
}

void LoopProcessModel::serialize(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

LayerModel* LoopProcessModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new LoopLayer{*this, viewModelId, parent};
}

LayerModel* LoopProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new LoopLayer{
                        deserializer, *this, parent};

        return autom;
    });
}

LayerModel* LoopProcessModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new LoopLayer{safe_cast<const LoopLayer&>(source), *this, newId, parent};
}



// MOVEME
template<>
void Visitor<Reader<DataStream>>::readFrom(const LoopProcessModel& proc)
{
    readFrom(static_cast<const BaseScenarioContainer&>(proc));

    readFrom(*proc.pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(LoopProcessModel& proc)
{
    writeTo(static_cast<BaseScenarioContainer&>(proc));

    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const LoopProcessModel& proc)
{
    readFrom(static_cast<const BaseScenarioContainer&>(proc));

    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(LoopProcessModel& proc)
{
    writeTo(static_cast<BaseScenarioContainer&>(proc));

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};
}

