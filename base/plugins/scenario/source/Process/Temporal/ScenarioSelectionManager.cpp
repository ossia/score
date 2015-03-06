#include "ScenarioSelectionManager.hpp"
#include "TemporalScenarioPresenter.hpp"
#include "TemporalScenarioViewModel.hpp"


#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventData.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include <QGraphicsScene>
#include <QApplication>

ScenarioSelectionManager::ScenarioSelectionManager(TemporalScenarioPresenter* presenter):
    QObject{presenter},
    m_presenter{presenter},
    m_selectionDispatcher{new iscore::SelectionDispatcher{this}},
    m_scenario{static_cast<ScenarioModel*>(presenter->m_viewModel->sharedProcessModel())}
{
    connect(presenter->m_view,       &TemporalScenarioView::scenarioPressed,
            this, &ScenarioSelectionManager::deselectAll);

    connect(presenter->m_view, &TemporalScenarioView::newSelectionArea,
            this, &ScenarioSelectionManager::setSelectionArea);
}

void ScenarioSelectionManager::deselectAll()
{
    // TODO Redo this correctly
    /*
    for(auto& event : m_events)
    {
        event->deselect();
    }

    for(auto& constraint : m_constraints)
    {
        constraint->deselect();
    }

    for(auto& timeNode : m_timeNodes)
    {
        timeNode->deselect();
    }*/
}

void ScenarioSelectionManager::setSelectionArea(const QRectF& area)
{
    QPainterPath path{};
    path.addRect(area);
    Selection sel{};
    for (auto item : m_presenter->m_view->scene()->items(path))
    {
        EventView* itemEv = dynamic_cast<EventView*>(item);
        auto itemCstr = dynamic_cast<TemporalConstraintView*>(item);
        auto itemTn = dynamic_cast<TimeNodeView*>(item);

        if (itemEv)
        {
            for (EventPresenter* event : m_presenter->m_events)
            {
                if (event->view() == itemEv)
                {
                    sel.push_back(event->model());
                    break;
                }
            }
        }
        else if (itemCstr)
        {
            for (TemporalConstraintPresenter* cstr : m_presenter->m_constraints)
            {
                if (view(cstr) == itemCstr)
                {
                    sel.push_back(viewModel(cstr)->model());
                    break;
                }
            }
        }

        else if (itemTn)
        {
            for (TimeNodePresenter* tn : m_presenter->m_timeNodes)
            {
                if (tn->view() == item)
                {
                    sel.push_back(tn->model());
                    break;
                }
            }
        }
    }
    m_selectionDispatcher->send(sel);
}
