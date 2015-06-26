#include "ScenarioFactory.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

QString ScenarioFactory::name() const
{
    return "Scenario";
}

Layer* ScenarioFactory::makeView(
        const LayerModel& viewmodel,
        QObject* parent)
{
    if(dynamic_cast<const TemporalScenarioViewModel*>(&viewmodel))
        return new TemporalScenarioView {static_cast<QGraphicsObject*>(parent) };

    return nullptr;
}

ProcessPresenter*
ScenarioFactory::makePresenter(
        const LayerModel& lm,
        Layer* view,
        QObject* parent)
{
    if(auto vm = dynamic_cast<const TemporalScenarioViewModel*>(&lm))
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
