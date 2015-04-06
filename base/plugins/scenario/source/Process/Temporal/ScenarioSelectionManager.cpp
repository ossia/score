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

#include "Document/BaseElement/BaseElementModel.hpp"

#include "source/Process/Temporal/TemporalScenarioView.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <QGraphicsScene>
using namespace iscore::IDocument;
ScenarioSelectionManager::ScenarioSelectionManager(TemporalScenarioPresenter* presenter):
    QObject{presenter},
    m_presenter{presenter},
    m_selectionDispatcher{documentFromObject(m_presenter->m_viewModel->sharedProcessModel())->selectionStack()},
    m_scenario{static_cast<ScenarioModel*>(m_presenter->m_viewModel->sharedProcessModel())}
{
    connect(m_presenter->m_view,       &TemporalScenarioView::scenarioPressed,
            this, [&] (QPointF)
    {
        // TODO state machine : only if we are not in a "create event" state
        deselectAll();
        focus();
    });

    connect(m_presenter->m_view, &TemporalScenarioView::newSelectionArea,
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
    // TODO check that they are in the current plug-in!!!!!!!
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
    m_selectionDispatcher.send(sel);
    focus();
}

void ScenarioSelectionManager::focus()
{
    m_presenter->focus();
}

void ScenarioSelectionManager::selectConstraint(TemporalConstraintPresenter *cstr)
{
    m_selectionDispatcher.send(Selection{cstr->model()});
}
