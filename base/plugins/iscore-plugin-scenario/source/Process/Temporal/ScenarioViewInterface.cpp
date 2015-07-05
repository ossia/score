#include "ScenarioViewInterface.hpp"

#include "TemporalScenarioPresenter.hpp"
#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include <QGraphicsScene>

ScenarioViewInterface::ScenarioViewInterface(TemporalScenarioPresenter* presenter) :
    QObject{presenter},
    m_presenter(presenter)
{
    connect(&m_presenter->m_viewModel, &TemporalScenarioLayer::eventMoved,
            this, &ScenarioViewInterface::on_eventMoved);

    connect(&m_presenter->m_viewModel, &TemporalScenarioLayer::constraintMoved,
            this, &ScenarioViewInterface::on_constraintMoved);
}

void ScenarioViewInterface::on_eventMoved(const id_type<EventModel>& eventId)
{

    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto rect = m_presenter->m_view->boundingRect();
    auto ev = m_presenter->m_events.at(eventId);

    ev->view()->setPos({(ev->model().date().msec() / m_presenter->m_zoomRatio),
                        rect.height() * ev->model().heightPercentage()
                       });

    auto timeNode = m_presenter->m_timeNodes.at(ev->model().timeNode());
    timeNode->view()->setPos({(timeNode->model().date().msec() / m_presenter->m_zoomRatio),
                              rect.height() * timeNode->model().y()});

    m_presenter->m_view->update();
    */
}

void ScenarioViewInterface::on_constraintMoved(const id_type<ConstraintModel>& constraintId)
{

    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto rect = m_presenter->m_view->boundingRect();
    auto msPerPixel = m_presenter->m_zoomRatio;

    TemporalConstraintPresenter* pres = m_presenter->m_constraints.at(constraintId);
    const auto& cstr_model = pres->model();

    auto startPos = cstr_model.startDate().toPixels(msPerPixel);
    auto delta = view(pres)->x() - startPos;
    bool dateChanged = (delta * delta > 1); // Magnetism

    if(dateChanged)
    {
        view(pres)->setPos({startPos,
                            rect.height() * cstr_model.heightPercentage()});
    }
    else
    {
        view(pres)->setY(qreal(rect.height() * cstr_model.heightPercentage()));
    }

    view(pres)->setDefaultWidth(cstr_model.defaultDuration().toPixels(msPerPixel));
    view(pres)->setMinWidth(cstr_model.minDuration().toPixels(msPerPixel));
    view(pres)->setMaxWidth(cstr_model.maxDuration().isInfinite(),
                            cstr_model.maxDuration().isInfinite()? -1 :
                                                                   cstr_model.maxDuration().toPixels(msPerPixel));

    auto endTn = m_presenter->m_events.at(cstr_model.endEvent())->model().timeNode();
    updateTimeNode(endTn);

    auto startTn = m_presenter->m_events.at(cstr_model.startEvent())->model().timeNode();
    updateTimeNode(startTn);

    m_presenter->m_view->update();
    */
}

void ScenarioViewInterface::on_timeNodeMoved(const TimeNodePresenter &timenode)
{
    auto h = m_presenter->m_view->boundingRect().height();
    timenode.view()->setExtent(timenode.model().extent().top * h,
                               timenode.model().extent().bottom * h);

    timenode.view()->setPos({timenode.model().date().msec() / m_presenter->m_zoomRatio,
                             timenode.model().extent().top * h});

    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_stateMoved(const id_type<DisplayedStateModel> &constraintId)
{

}

template<typename T>
void update_min_max(const T& val, T& min, T& max)
{
    min = val < min ? val : min;
    max = val > max ? val : max;
}

void ScenarioViewInterface::addPointInEvent(const id_type<EventModel> &eventId, double y)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto event = m_presenter->m_events.at(eventId);
    auto h = m_presenter->m_view->boundingRect().height();
    event->view()->addPoint(int (h * (y - event->model().heightPercentage())));

    auto tn = m_presenter->m_timeNodes.at(event->model().timeNode());
    tn->view()->addPoint(int(h * (y - tn->model().y()) ));
    */
}

void ScenarioViewInterface::on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    const auto& constraint = m_presenter->m_constraints.at(constraintId)->model();
    EventPresenter* start = m_presenter->m_events.at(constraint.startEvent());
    start->view()->setShadow(enter);
    EventPresenter* end = m_presenter->m_events.at(constraint.endEvent());
    end->view()->setShadow(enter);
    */
}

void ScenarioViewInterface::on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter)
{

    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    const auto& event = m_presenter->m_events.at(eventId)->model();
    for (const auto& cstr : event.constraints())
    {
        auto cstrView = view(m_presenter->m_constraints.at(cstr));
        cstrView->setShadow(enter);
    }
    */
}
