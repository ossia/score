#include "ScenarioCommandManager.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"
#include "TemporalScenarioPresenter.hpp"

#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventData.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"

#include "Commands/Scenario/CreateEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/CreateConstraint.hpp"
#include "Commands/Scenario/RemoveConstraint.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveTimeNode.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/RemoveMultipleElements.hpp"

#include <core/interface/document/DocumentInterface.hpp>

#include <algorithm>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>

using namespace Scenario::Command;

template<typename InputVector, typename OutputVector>
void copyIfSelected(const InputVector& in, OutputVector& out)
{
    std::copy_if(begin(in),
                 end(in),
                 back_inserter(out),
                 [](typename InputVector::value_type c)
    {
        return c->isSelected();
    });
}




ScenarioCommandManager::ScenarioCommandManager(TemporalScenarioPresenter* presenter) :
    m_presenter(presenter)
{

}


// Three cases :
// We are between :
//  an event and nothing -> CreateEventAfterEvent
//  an event and a timenode -> CreateEventAfterEventOnTimeNode
//  an event and another event -> CreateConstraint

void ScenarioCommandManager::createConstraint(EventData data)
{
    using namespace std;
    data.dDate.setMSecs(data.x * m_presenter->m_millisecPerPixel - model(m_presenter->m_viewModel)->event(data.eventClickedId)->date().msec());
    data.relativeY = data.y / m_presenter->m_view->boundingRect().height();

    auto cmdPath = iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel());

    // We rollback so that we don't get polluted
    // by the "fake" created events / timenodes.
    if(m_ongoingCommand)
    {
        rollbackOngoingCommand();
    }

    QList<EventPresenter*> collidingEvents;
    copy_if(begin(m_presenter->m_events), end(m_presenter->m_events), back_inserter(collidingEvents),
            [](EventPresenter * ev)
    {
        return ev->view()->isUnderMouse();
    });

    QList<TimeNodePresenter*> collidingTimeNodes;
    copy_if(begin(m_presenter->m_timeNodes), end(m_presenter->m_timeNodes), back_inserter(collidingTimeNodes),
            [](TimeNodePresenter * tn)
    {
        return tn->view()->isUnderMouse();
    });

    if(collidingEvents.empty())
    {
        if(collidingTimeNodes.empty())
        {
            sendOngoingCommand(new CreateEventAfterEvent(move(cmdPath), data));
        }
        else
        {
            auto tn = collidingTimeNodes.first();
            data.endTimeNodeId = tn->id();
            data.dDate = tn->model()->date() - model(m_presenter->m_viewModel)->event(data.eventClickedId)->date();

            sendOngoingCommand(new CreateEventAfterEventOnTimeNode(move(cmdPath), data));
        }
    }
    else
    {
        sendOngoingCommand(new CreateConstraint(move(cmdPath),
                                                data.eventClickedId,
                                                collidingEvents.first()->id()));
    }
}

void ScenarioCommandManager::on_scenarioReleased(QPointF point, QPointF scenePoint)
{
    EventData data {};
    data.eventClickedId = m_presenter->m_events.back()->id();
    data.x = point.x();
    data.dDate.setMSecs(point.x() * m_presenter->m_millisecPerPixel);
    data.y = point.y();
    data.relativeY = point.y() /  m_presenter->m_view->boundingRect().height();
    data.scenePos = scenePoint;

    TimeNodeView* tnv =  dynamic_cast<TimeNodeView*>(m_presenter->m_view->scene()->itemAt(scenePoint, QTransform()));

    if(tnv)
    {
        for(auto timeNode : m_presenter->m_timeNodes)
        {
            if(timeNode->view() == tnv)
            {
                data.endTimeNodeId = timeNode->id();
                data.dDate = timeNode->model()->date();
                data.x = data.dDate.msec() / m_presenter->m_millisecPerPixel;
                break;
            }
        }
    }

    auto cmd = new Scenario::Command::CreateEvent(iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
            data);
    m_presenter->submitCommand(cmd);
}

void ScenarioCommandManager::clearContentFromSelection()
{
    // 1. Select items
    std::vector<TemporalConstraintPresenter*> constraintsToRemove;
    std::vector<EventPresenter*> eventsToRemove;

    copyIfSelected(m_presenter->m_constraints, constraintsToRemove);
    copyIfSelected(m_presenter->m_events, eventsToRemove);

    QVector<iscore::SerializableCommand*> commands;

    // 3. Create a Delete command for each. For now : only emptying.
    for(auto& constraint : constraintsToRemove)
    {
        commands.push_back(
            new ClearConstraint(
                iscore::IDocument::path(viewModel(constraint)->model())));
    }

    for(auto& event : eventsToRemove)
    {
        commands.push_back(
            new ClearEvent(
                iscore::IDocument::path(event->model())));
    }

    // 4. Make a meta-command that binds them all and calls undo & redo on the queue.
    auto cmd = new RemoveMultipleElements {std::move(commands) };
    emit m_presenter->submitCommand(cmd);
}

void ScenarioCommandManager::deleteSelection()
{
    // TODO quelques comportements bizarres à régler ...

    //*
    // 1. Select items
    std::vector<TemporalConstraintPresenter*> constraintsToRemove;
    std::vector<EventPresenter*> eventsToRemove;

    copyIfSelected(m_presenter->m_constraints, constraintsToRemove);
    copyIfSelected(m_presenter->m_events, eventsToRemove);

    if(constraintsToRemove.size() != 0 || eventsToRemove.size() != 0)
    {
        QVector<iscore::SerializableCommand*> commands;

        // 2. Create a Delete command for each. For now : only emptying.
        for(auto& constraint : constraintsToRemove)
        {
            commands.push_back(
                new RemoveConstraint(
                    iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
                    constraint->abstractConstraintViewModel()->model()));
        }

        for(auto& event : eventsToRemove)
        {
            commands.push_back(
                new RemoveEvent(
                    iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
                    event->model()));

        }

        // 3. Make a meta-command that binds them all and calls undo & redo on the queue.
        auto cmd = new RemoveMultipleElements {std::move(commands) };

        if(cmd)
        {
            emit m_presenter->submitCommand(cmd);
        }
    }

    // */
}

void ScenarioCommandManager::moveEventAndConstraint(EventData data)
{
    data.dDate.setMSecs(data.x * m_presenter->m_millisecPerPixel);
    data.relativeY = data.y / m_presenter->m_view->boundingRect().height();

    auto cmd = new MoveEvent(iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
                             data);

    sendOngoingCommand(cmd);
}

void ScenarioCommandManager::moveConstraint(ConstraintData data)
{
    data.dDate.setMSecs(data.x * m_presenter->m_millisecPerPixel);
    data.relativeY = data.y / m_presenter->m_view->boundingRect().height();

    auto cmd = new MoveConstraint(iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
                                  data);


    sendOngoingCommand(cmd);
}

void ScenarioCommandManager::moveTimeNode(EventData data)
{
    auto ev = findById(m_presenter->m_events, data.eventClickedId);
    data.y = ev->view()->y();
    data.dDate.setMSecs(data.x * m_presenter->m_millisecPerPixel);
    data.relativeY = data.y / m_presenter->m_view->boundingRect().height();


    auto cmd = new MoveTimeNode(iscore::IDocument::path(m_presenter->m_viewModel->sharedProcessModel()),
                                data);

    sendOngoingCommand(cmd);
}

void ScenarioCommandManager::sendOngoingCommand(iscore::SerializableCommand* cmd)
{
    if(m_ongoingCommand && cmd->id() != m_ongoingCommandId)
    {
        rollbackOngoingCommand();
    }

    auto doc = iscore::IDocument::documentFromObject(m_presenter->m_viewModel->sharedProcessModel());

    if(!m_ongoingCommand)
    {
        m_ongoingCommand = true;
        m_ongoingCommandId = cmd->id();
        doc->presenter()->initiateOngoingCommand(cmd, m_presenter->m_viewModel->sharedProcessModel());
    }
    else
    {
        doc->presenter()->continueOngoingCommand(cmd);
    }
}

void ScenarioCommandManager::finishOngoingCommand()
{
    auto doc = iscore::IDocument::documentFromObject(m_presenter->m_viewModel->sharedProcessModel());
    m_ongoingCommand = false;
    doc->presenter()->validateOngoingCommand();
    m_ongoingCommandId = -1;
}

void ScenarioCommandManager::rollbackOngoingCommand()
{
    auto doc = iscore::IDocument::documentFromObject(m_presenter->m_viewModel->sharedProcessModel());
    doc->presenter()->rollbackOngoingCommand();
    m_ongoingCommand = false;
    m_ongoingCommandId = -1;
}

void ScenarioCommandManager::on_ctrlStateChanged(bool ctrlPressed)
{
    if(!m_ongoingCommand)
    {
        return;
    }

    rollbackOngoingCommand();

    if(ctrlPressed)
    {
        createConstraint(m_presenter->m_lastData);
    }
    else
    {
        moveEventAndConstraint(m_presenter->m_lastData);
    }
}
