#include "ScenarioFactory.hpp"

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>

ScenarioFactory::ScenarioFactory(Scenario::EditionSettings& e):
    m_editionSettings{e}
{

}

LayerView* ScenarioFactory::makeLayerView(
        const LayerModel& viewmodel,
        QGraphicsItem* parent)
{
    if(dynamic_cast<const TemporalScenarioLayerModel*>(&viewmodel))
        return new TemporalScenarioView {parent};

    return nullptr;
}

LayerPresenter*
ScenarioFactory::makeLayerPresenter(
        const LayerModel& lm,
        LayerView* view,
        QObject* parent)
{
    if(auto vm = dynamic_cast<const TemporalScenarioLayerModel*>(&lm))
    {
        auto pres = new TemporalScenarioPresenter {
                iscore::IDocument::documentContext(lm.processModel()),
                m_editionSettings,
                *vm,
                view,
                parent};
        static_cast<TemporalScenarioView*>(view)->setPresenter(pres);
        return pres;
    }
    return nullptr;
}

const ProcessFactoryKey& ScenarioFactory::key_impl() const
{
    return ScenarioProcessMetadata::factoryKey();
}

QString ScenarioFactory::prettyName() const
{
    return ScenarioProcessMetadata::factoryPrettyName();
}

Process* ScenarioFactory::makeModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent)
{
    return new ScenarioModel {duration, id, parent};
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
