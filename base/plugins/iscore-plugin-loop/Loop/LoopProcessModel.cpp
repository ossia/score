#include "LoopProcessModel.hpp"
#include <Loop/LoopLayer.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>

namespace Loop
{
ProcessModel::ProcessModel(
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

    ConstraintDurations::Algorithms::changeAllDurations(constraint(), duration);
    endEvent().setDate(duration);
    endTimeNode().setDate(duration);

    const double height = 0.15;
    constraint().setHeightPercentage(height);
    constraint().metadata.setName("Loop pattern");
    constraint().metadata.setColor(Qt::yellow);
    BaseScenarioContainer::startState().setHeightPercentage(height);
    BaseScenarioContainer::endState().setHeightPercentage(height);
    BaseScenarioContainer::startEvent().setExtent({height, 0.2});
    BaseScenarioContainer::endEvent().setExtent({height, 0.2});
    BaseScenarioContainer::startTimeNode().setExtent({height, 1});
    BaseScenarioContainer::endTimeNode().setExtent({height, 1});
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process>& id,
        QObject* parent):
    Process{source.duration(), id, LoopProcessMetadata::processObjectName(), parent},
    BaseScenarioContainer{this}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
                      this};
}

ProcessModel* ProcessModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new ProcessModel{*this, newId, newParent};
}

QString ProcessModel::prettyName() const
{
    return "Loop Process";
}

QByteArray ProcessModel::makeLayerConstructionData() const
{
    return {};
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::startExecution()
{
}

void ProcessModel::stopExecution()
{
}

void ProcessModel::reset()
{
}

ProcessStateDataInterface* ProcessModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* ProcessModel::endStateData() const
{
    return nullptr;
}

Selection ProcessModel::selectableChildren() const
{
    Selection s;

    for_each_in_tuple(elements(), [&] (auto elt) { s.append(elt); });

    return s;
}

Selection ProcessModel::selectedChildren() const
{
    Selection s;

    for_each_in_tuple(elements(), [&] (auto elt) {
        if(elt->selection.get())
            s.append(elt);
    });

    return s;
}

void ProcessModel::setSelection(const Selection& s) const
{
    for_each_in_tuple(elements(), [&] (auto elt) {
        elt->selection.set(s.contains(elt)); // OPTIMIZEME
    });
}

void ProcessModel::serialize(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

LayerModel* ProcessModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new LoopLayer{*this, viewModelId, parent};
}

LayerModel* ProcessModel::loadLayer_impl(
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

LayerModel* ProcessModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new LoopLayer{safe_cast<const LoopLayer&>(source), *this, newId, parent};
}

const QVector<Id<ConstraintModel> > constraintsBeforeTimeNode(
        const ProcessModel& scen,
        const Id<TimeNodeModel>& timeNodeId)
{
    if(timeNodeId == scen.endTimeNode().id())
    {
        return {scen.constraint().id()};
    }
    return {};
}



}


// MOVEME
template<>
void Visitor<Reader<DataStream>>::readFrom(const Loop::ProcessModel& proc)
{
    readFrom(static_cast<const BaseScenarioContainer&>(proc));

    readFrom(*proc.pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Loop::ProcessModel& proc)
{
    writeTo(static_cast<BaseScenarioContainer&>(proc));

    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Loop::ProcessModel& proc)
{
    readFrom(static_cast<const BaseScenarioContainer&>(proc));

    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Loop::ProcessModel& proc)
{
    writeTo(static_cast<BaseScenarioContainer&>(proc));

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};
}
template<>
QString NameInUndo<Loop::ProcessModel>()
{
    return "Loop";
}
