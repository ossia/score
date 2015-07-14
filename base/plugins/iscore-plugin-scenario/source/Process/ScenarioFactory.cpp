#include "ScenarioFactory.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioLayer.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

QString ScenarioFactory::name() const
{
    return "Scenario";
}

Layer* ScenarioFactory::makeView(
        const LayerModel& viewmodel,
        QObject* parent)
{
    if(dynamic_cast<const TemporalScenarioLayer*>(&viewmodel))
        return new TemporalScenarioView {static_cast<QGraphicsObject*>(parent) };

    return nullptr;
}

ProcessPresenter*
ScenarioFactory::makePresenter(
        const LayerModel& lm,
        Layer* view,
        QObject* parent)
{
    if(auto vm = dynamic_cast<const TemporalScenarioLayer*>(&lm))
    {
        auto pres = new TemporalScenarioPresenter {*vm, view, parent};
        static_cast<TemporalScenarioView*>(view)->setPresenter(pres);
        return pres;
    }
    return nullptr;
}

ProcessModel* ScenarioFactory::makeModel(
        const TimeValue& duration,
        const id_type<ProcessModel>& id,
        QObject* parent)
{
    return new ScenarioModel {duration, id, parent};
}

QByteArray ScenarioFactory::makeStaticLayerConstructionData() const
{
    // Like ScenarioModel::makeViewModelConstructionData but without data since
    // there won't be constraints at the beginning.
    QMap<id_type<ConstraintModel>, id_type<ConstraintViewModel>> map;

    QByteArray arr;
    QDataStream s{&arr, QIODevice::WriteOnly};
    s << map;

    return arr;
}
