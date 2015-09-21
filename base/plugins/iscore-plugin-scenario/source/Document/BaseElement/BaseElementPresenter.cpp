#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/DoubleSlider.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp"
#include "Document/TimeRuler/LocalTimeRuler/LocalTimeRulerPresenter.hpp"
#include "Widgets/ProgressBar.hpp"

#include <ProcessInterface/LayerModel.hpp>

#include "Document/Constraint/ConstraintModel.hpp"

#include <ProcessInterface/Process.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QApplication>

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "BaseScenario/BaseScenarioStateMachine.hpp"

#include "ZoomPolicy.hpp"

using namespace iscore;

BaseElementModel& BaseElementPresenter::model() const
{
    return static_cast<BaseElementModel&>(*m_model);
}

ZoomRatio BaseElementPresenter::zoomRatio() const
{
    return m_zoomRatio;
}

BaseElementView* BaseElementPresenter::view() const
{
    return static_cast<BaseElementView*>(m_view);
}

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
                                           DocumentDelegateModelInterface* delegate_model,
                                           DocumentDelegateViewInterface* delegate_view) :
    DocumentDelegatePresenterInterface {parent_presenter,
                                        "BaseElementPresenter",
                                        delegate_model,
                                        delegate_view},
    m_scenarioPresenter{new DisplayedElementsPresenter{this}},
    m_selectionDispatcher{IDocument::documentFromObject(model())->selectionStack()},
    m_mainTimeRuler{new TimeRulerPresenter{view()->timeRuler(), this}}/*,
    m_localTimeRuler { new LocalTimeRulerPresenter{view()->localTimeRuler(), this}}*/
{
    // Setup the connections
    con((m_selectionDispatcher.stack()), &SelectionStack::currentSelectionChanged,
            this,                             &BaseElementPresenter::on_newSelection);
    connect(view(), &BaseElementView::horizontalZoomChanged,
            this,   &BaseElementPresenter::on_zoomSliderChanged);
    connect(view()->view(), &SizeNotifyingGraphicsView::sizeChanged,
            this,           &BaseElementPresenter::on_viewSizeChanged);
    connect(view()->view(), &SizeNotifyingGraphicsView::zoom,
            this,  &BaseElementPresenter::on_zoomOnWheelEvent);
    connect(view(), &BaseElementView::horizontalPositionChanged,
            this,   &BaseElementPresenter::on_horizontalPositionChanged);
    con(model(), &BaseElementModel::focusMe,
            this,    [&] () { view()->view()->setFocus(); });

    con(model().baseConstraint().duration, &ConstraintDurations::defaultDurationChanged,
            m_mainTimeRuler, &TimeRulerPresenter::setDuration);

    // Setup of the state machine.
    m_stateMachine = new BaseScenarioStateMachine{this};

    // Show our constraint
    con(model(), &BaseElementModel::displayedConstraintChanged,
            this, &BaseElementPresenter::on_displayedConstraintChanged);

    model().setDisplayedConstraint(&model().baseConstraint());

    // Progress bar, time rules

    m_mainTimeRuler->setDuration(model().baseConstraint().duration.defaultDuration());
    /*
    m_localTimeRuler->setDuration(model().baseConstraint().duration.defaultDuration());
    */
}

const ConstraintModel& BaseElementPresenter::displayedConstraint() const
{
    return model().displayedElements.displayedConstraint();
}

void BaseElementPresenter::on_askUpdate()
{
    view()->update();
}

void BaseElementPresenter::selectAll()
{
    auto processmodel = model().focusManager().focusedModel();
    if(processmodel)
    {
        m_selectionDispatcher.setAndCommit(processmodel->selectableChildren());
    }
}

void BaseElementPresenter::deselectAll()
{
    m_selectionDispatcher.setAndCommit(Selection{});
}

void BaseElementPresenter::setDisplayedObject(const ObjectPath &path)
{
    if(path.vec().last().objectName().contains("ConstraintModel")) // Constraint & BaseConstraint
    {
        model().setDisplayedConstraint(&path.find<ConstraintModel>());
    }
}

void BaseElementPresenter::on_displayedConstraintChanged()
{
    m_scenarioPresenter->on_displayedConstraintChanged(displayedConstraint());
    connect(m_scenarioPresenter->constraintPresenter(), &FullViewConstraintPresenter::objectSelected,
            this,				  &BaseElementPresenter::setDisplayedObject);

    // Set a new zoom ratio, such that the displayed constraint takes the whole screen.

    auto newZoom = displayedConstraint().fullView()->zoom();

    double newSliderPos = ZoomPolicy::zoomRatioToSliderPos(
                              newZoom,
                              displayedConstraint().duration.defaultDuration().msec(),
                              view()->view()->width()
                              );
    view()->zoomSlider()->setValue(newSliderPos);

    auto center = displayedConstraint().fullView()->center();

    setMillisPerPixel(newZoom);
    view()->view()->centerOn(center);

    on_askUpdate();
}

void BaseElementPresenter::setMillisPerPixel(ZoomRatio newRatio)
{
    m_zoomRatio = newRatio;

    m_mainTimeRuler->setPixelPerMillis(1.0 / m_zoomRatio);
    /*
    m_localTimeRuler->setPixelPerMillis(1.0 / m_zoomRatio);
    */
    m_scenarioPresenter->on_zoomRatioChanged(m_zoomRatio);
}

void BaseElementPresenter::on_newSelection(const Selection& sel)
{
    /*
    int scroll = m_localTimeRuler->totalScroll();
    delete m_localTimeRuler;
    view()->newLocalTimeRuler();
    m_localTimeRuler = new LocalTimeRulerPresenter{view()->localTimeRuler(), this};
    m_localTimeRuler->scroll(scroll);
    m_localTimeRuler->setPixelPerMillis(1./m_zoomRatio);

    if (sel.empty())
    {
        m_localTimeRuler->setDuration(TimeValue::zero());
        m_localTimeRuler->setStartPoint(TimeValue::zero());
    }
    else
    {
        if(auto cstr = dynamic_cast<const ConstraintModel*>(*sel.begin()) )
        {
            m_localTimeRuler->setDuration(cstr->duration.defaultDuration());
            m_localTimeRuler->setStartPoint(cstr->startDate());

            connect(cstr,               &ConstraintModel::defaultDurationChanged,
                    m_localTimeRuler,   &LocalTimeRulerPresenter::setDuration);
            connect(cstr,               &ConstraintModel::startDateChanged,
                    m_localTimeRuler,   &LocalTimeRulerPresenter::setStartPoint);
        }
    }*/
}

void BaseElementPresenter::on_zoomSliderChanged(double sliderPos)
{
    auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
                               sliderPos,
                               displayedConstraint().duration.defaultDuration().msec(),
                               view()->view()->width()
                               );

    updateZoom(newMillisPerPix, QPointF(0,0));
}

void BaseElementPresenter::on_zoomOnWheelEvent(QPoint zoom, QPointF center)
{
    // convert the mouse displacement into a fake slider move

    double zoomSpeed = 1.5; // experiment value
    double zoomratio = (view()->zoomSlider()->value() +
                        zoomSpeed * float(zoom.y())/float(view()->zoomSlider()->width()));

    if (zoomratio > 1.)
        zoomratio = 0.99;
    else if(zoomratio < 0.)
        zoomratio = 0.01;

    view()->zoomSlider()->setValue(zoomratio);

    auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
                               zoomratio,
                               displayedConstraint().duration.defaultDuration().msec(),
                               view()->view()->width()
                               );

    updateZoom(newMillisPerPix, center);

}

void BaseElementPresenter::on_viewSizeChanged(const QSize &s)
{
    on_zoomSliderChanged(view()->zoomSlider()->value());
}

void BaseElementPresenter::on_horizontalPositionChanged(int dx)
{
    QRect viewport_rect = view()->view()->viewport()->rect() ;
    QRectF visible_scene_rect = view()->view()->mapToScene(viewport_rect).boundingRect();

    m_mainTimeRuler->scroll(visible_scene_rect.left());
    /*
    m_localTimeRuler->scroll(dx);
    */
}

void BaseElementPresenter::updateRect(const QRectF& rect)
{
    view()->view()->setSceneRect(rect);

    /*
    QRectF other{view()->rulerView()->sceneRect()};
    other.setWidth(rect.width());
    other.setX(rect.x());
    view()->rulerView()->setSceneRect(other);
    */
}

void BaseElementPresenter::updateZoom(ZoomRatio newZoom, QPointF focus)
{
    auto w = view()->view()->viewport()->width();
    auto h = view()->view()->viewport()->height();

    QRect viewport_rect = view()->view()->viewport()->rect() ;
    QRectF visible_scene_rect = view()->view()->mapToScene(viewport_rect).boundingRect();

    qreal center = focus.x();
    if (focus.isNull())
        center = visible_scene_rect.center().x();
    else if (focus.x() - visible_scene_rect.left() < 40)
        center = visible_scene_rect.left();
    else if (visible_scene_rect.right() - focus.x() < 40)
        center = visible_scene_rect.right();



    qreal centerT = center * m_zoomRatio; // here's the old zoom

    auto deltaX = center - visible_scene_rect.left();

    auto y = visible_scene_rect.top();

    if(newZoom != m_zoomRatio)
        setMillisPerPixel(newZoom);


    qreal x = centerT/m_zoomRatio - deltaX;; // here's the new zoom

    auto newView = QRectF{x, y,(qreal)w, (qreal)h};

    view()->view()->ensureVisible(newView,0,0);

    QRectF new_visible_scene_rect = view()->view()->mapToScene(viewport_rect).boundingRect();

    displayedConstraint().fullView()->setZoom(m_zoomRatio);
    displayedConstraint().fullView()->setCenter(new_visible_scene_rect.center());
}


