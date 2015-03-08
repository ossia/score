#include "ScenarioFactory.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

QString ScenarioFactory::name() const
{
    return "Scenario";
}

QStringList ScenarioFactory::availableViews()
{
    return {"Temporal"};
}

ProcessViewInterface* ScenarioFactory::makeView(QString view, QObject* parent)
{
    if(view == "Temporal")
        return new TemporalScenarioView {static_cast<QGraphicsObject*>(parent) };

    return nullptr;
}

ProcessPresenterInterface*
ScenarioFactory::makePresenter(ProcessViewModelInterface* pvm,
                               ProcessViewInterface* view,
                               QObject* parent)
{
    return new TemporalScenarioPresenter {pvm, view, parent};
}

ProcessSharedModelInterface* ScenarioFactory::makeModel(id_type<ProcessSharedModelInterface> id, QObject* parent)
{
    return new ScenarioModel {id, parent};
}

#include <public_interface/serialization/DataStreamVisitor.hpp>
