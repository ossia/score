#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <iscore/tools/std/Optional.hpp>
#include <QDataStream>
#include <QIODevice>
#include <QMap>

#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include "ScenarioFactory.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process { class LayerPresenter; }
class LayerView;
class QGraphicsItem;
class QObject;

namespace Scenario
{
class ConstraintModel;
class ConstraintViewModel;
ScenarioFactory::ScenarioFactory(Scenario::EditionSettings& e):
    m_editionSettings{e}
{

}

Process::LayerView* ScenarioFactory::makeLayerView(
        const Process::LayerModel& viewmodel,
        QGraphicsItem* parent)
{
    if(dynamic_cast<const TemporalScenarioLayerModel*>(&viewmodel))
        return new TemporalScenarioView {parent};

    return nullptr;
}

Process::LayerPresenter*
ScenarioFactory::makeLayerPresenter(
        const Process::LayerModel& lm,
        Process::LayerView* view,
        const Process::ProcessPresenterContext& context,
        QObject* parent)
{
    if(auto vm = dynamic_cast<const TemporalScenarioLayerModel*>(&lm))
    {
        auto pres = new TemporalScenarioPresenter {
                m_editionSettings,
                *vm,
                view,
                context,
                parent};
        static_cast<TemporalScenarioView*>(view)->setPresenter(pres);
        return pres;
    }
    return nullptr;
}

const UuidKey<Process::ProcessFactory>& ScenarioFactory::concreteFactoryKey() const
{
    return Metadata<ConcreteFactoryKey_k, Scenario::ProcessModel>::get();
}

QString ScenarioFactory::prettyName() const
{
    return Metadata<PrettyName_k, Scenario::ProcessModel>::get();
}

Process::ProcessModel* ScenarioFactory::makeModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent)
{
    return new Scenario::ProcessModel {duration, id, parent};
}

QByteArray ScenarioFactory::makeStaticLayerConstructionData() const
{
    // Like ScenarioModel::makeViewModelConstructionData but without data since
    // there won't be constraints at the beginning.
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;

    QByteArray arr;
    QDataStream s{&arr, QIODevice::WriteOnly};
    s << map;

    return arr;
}

QByteArray ScenarioFactory::makeLayerConstructionData(
        const Process::ProcessModel& proc) const
{
    auto& scenar = static_cast<const Scenario::ProcessModel&>(proc);
    // For all existing constraints we need to generate corresponding
    // view models ids. One day we may need to do this for events / time nodes too.
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    std::vector<Id<ConstraintViewModel>> vec;
    vec.reserve(scenar.constraints.size());
    for(const auto& constraint : scenar.constraints)
    {
        auto id = getStrongId(vec);
        vec.push_back(id);
        map.insert(constraint.id(), id);
    }

    QByteArray arr;
    QDataStream s{&arr, QIODevice::WriteOnly};
    s << map;
    return arr;
}

Process::LayerModel* ScenarioFactory::makeLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto& scenar = static_cast<Scenario::ProcessModel&>(proc);
    auto scen = new TemporalScenarioLayerModel {
                viewModelId, map,
                scenar, parent};
    scenar.setupLayer(scen);
    return scen;
}


Process::LayerModel* ScenarioFactory::cloneLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    auto& scenar = static_cast<Scenario::ProcessModel&>(proc);
    auto scen = new TemporalScenarioLayerModel{
                static_cast<const TemporalScenarioLayerModel&>(source),
                newId,
                scenar,
                parent};
    scenar.setupLayer(scen);
    return scen;
}

Process::LayerModel* ScenarioFactory::loadLayer_impl(
        Process::ProcessModel& proc,
        const VisitorVariant& vis,
        QObject* parent)
{
    auto& scenar = static_cast<Scenario::ProcessModel&>(proc);
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto scen = new TemporalScenarioLayerModel{
                    deserializer, scenar, parent};
        scenar.setupLayer(scen);
        return scen;
    });
}

}
