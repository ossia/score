#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <boost/optional/optional.hpp>
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
    return new Scenario::ScenarioModel {duration, id, parent};
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
}
