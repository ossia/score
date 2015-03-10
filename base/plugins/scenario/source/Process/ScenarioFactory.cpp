#include "ScenarioFactory.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

QString ScenarioFactory::name() const
{
    return "Scenario";
}

ProcessViewInterface* ScenarioFactory::makeView(ProcessViewModelInterface* viewmodel, QObject* parent)
{
    if(auto tvm = dynamic_cast<TemporalScenarioViewModel*>(viewmodel))
        return new TemporalScenarioView {static_cast<QGraphicsObject*>(parent) };

    return nullptr;
}

ProcessPresenterInterface*
ScenarioFactory::makePresenter(ProcessViewModelInterface* pvm,
                               ProcessViewInterface* view,
                               QObject* parent)
{
    if(auto vm = dynamic_cast<TemporalScenarioViewModel*>(pvm))
        return new TemporalScenarioPresenter {vm, view, parent};

    return nullptr;
}

ProcessSharedModelInterface* ScenarioFactory::makeModel(id_type<ProcessSharedModelInterface> id, QObject* parent)
{
    return new ScenarioModel {id, parent};
}

#include <iscore/serialization/DataStreamVisitor.hpp>
