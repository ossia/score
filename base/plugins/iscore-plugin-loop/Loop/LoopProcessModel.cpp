#include <Loop/LoopLayer.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <boost/optional/optional.hpp>
#include <qnamespace.h>
#include <algorithm>
#include <tuple>

#include "Loop/LoopProcessMetadata.hpp"
#include "LoopProcessModel.hpp"
#include <Process/Style/Skin.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;

namespace Loop
{

ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    Scenario::BaseScenarioContainer{this}
{
    this->setUseParentDuration(true);
    pluginModelList = new iscore::ElementPluginModelList{
                      iscore::IDocument::documentContext(*parent),
                      this};

    Scenario::BaseScenarioContainer::init();

    Scenario::ConstraintDurations::Algorithms::changeAllDurations(constraint(), duration);
    endEvent().setDate(duration);
    endTimeNode().setDate(duration);

    const double height = 0.15;
    constraint().setHeightPercentage(height);
    constraint().metadata.setName("Loop pattern");
    constraint().metadata.setColor(ScenarioStyle::instance().ConstraintInvalid);
    BaseScenarioContainer::startState().setHeightPercentage(height);
    BaseScenarioContainer::endState().setHeightPercentage(height);
    BaseScenarioContainer::startEvent().setExtent({height, 0.2});
    BaseScenarioContainer::endEvent().setExtent({height, 0.2});
    BaseScenarioContainer::startTimeNode().setExtent({height, 1});
    BaseScenarioContainer::endTimeNode().setExtent({height, 1});
}

ProcessModel::ProcessModel(
        const Loop::ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{source.duration(), id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    BaseScenarioContainer{this}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
                      this};

    BaseScenarioContainer::init(source);
}

ProcessModel::~ProcessModel()
{
    delete pluginModelList;
}

ProcessModel* ProcessModel::clone(
        const Id<Process::ProcessModel>& newId,
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
    constraint().startExecution();
}

void ProcessModel::stopExecution()
{
    constraint().stopExecution();
}

void ProcessModel::reset()
{
    constraint().reset();
    startEvent().reset();
    endEvent().reset();
    // TODO reset events / states display too
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

void ProcessModel::serialize_impl(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

Process::LayerModel* ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new Layer{*this, viewModelId, parent};
}

Process::LayerModel* ProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new Layer{
                        deserializer, *this, parent};

        return autom;
    });
}

Process::LayerModel* ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    return new Layer{safe_cast<const Layer&>(source), *this, newId, parent};
}

const QVector<Id<Scenario::ConstraintModel> > constraintsBeforeTimeNode(
        const ProcessModel& scen,
        const Id<Scenario::TimeNodeModel>& timeNodeId)
{
    if(timeNodeId == scen.endTimeNode().id())
    {
        return {scen.constraint().id()};
    }
    return {};
}

}
