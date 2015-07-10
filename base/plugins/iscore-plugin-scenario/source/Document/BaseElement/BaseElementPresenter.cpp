#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/DoubleSlider.hpp"
#include "Document/BaseElement/Widgets/AddressBar.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp"
#include "Document/TimeRuler/LocalTimeRuler/LocalTimeRulerPresenter.hpp"
#include "Widgets/ProgressBar.hpp"

#include <ProcessInterface/LayerModel.hpp>

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"

#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QApplication>
#include "StateMachine/BaseMoveSlot.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "BaseScenario/BaseElementStateMachine.hpp"
using namespace iscore;

BaseElementModel* BaseElementPresenter::model() const
{
    return static_cast<BaseElementModel*>(m_model);
}

ZoomRatio BaseElementPresenter::zoomRatio() const
{
    return m_millisecondsPerPixel;
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
    m_progressBar{new ProgressBar},
    m_mainTimeRuler{new TimeRulerPresenter{view()->timeRuler(), this}},
    m_localTimeRuler { new LocalTimeRulerPresenter{view()->localTimeRuler(), this}}
{
    // Setup the connections
    connect(&(m_selectionDispatcher.stack()), &SelectionStack::currentSelectionChanged,
            this,                             &BaseElementPresenter::on_newSelection);
    connect(view()->addressBar(), &AddressBar::objectSelected,
            this,				  &BaseElementPresenter::setDisplayedObject);
    connect(view(), &BaseElementView::horizontalZoomChanged,
            this,   &BaseElementPresenter::on_zoomSliderChanged);
    connect(view()->view(), &SizeNotifyingGraphicsView::sizeChanged,
            this,           &BaseElementPresenter::on_viewSizeChanged);
    connect(view()->view(), &SizeNotifyingGraphicsView::zoom,
            this,  &BaseElementPresenter::on_zoomOnWheelEvent);
    connect(view(), &BaseElementView::horizontalPositionChanged,
            this,   &BaseElementPresenter::on_horizontalPositionChanged);
    connect(model(), &BaseElementModel::focusMe,
            this,    [&] () { view()->view()->setFocus(); });


    // Setup of the state machine.
    m_stateMachine = new BaseElementStateMachine{this};

    // Show our constraint
    connect(model(), &BaseElementModel::displayedConstraintChanged,
            this, &BaseElementPresenter::on_displayedConstraintChanged);

    model()->setDisplayedConstraint(model()->baseConstraint());

    // Progress bar, time rules
    view()->scene()->addItem(m_progressBar);
    setProgressBarTime(std::chrono::milliseconds{0});
    m_mainTimeRuler->setDuration(model()->baseConstraint()->defaultDuration());
    m_localTimeRuler->setDuration(model()->baseConstraint()->defaultDuration());
}

const ConstraintModel& BaseElementPresenter::displayedConstraint() const
{
    return model()->displayedElements.displayedConstraint();
}

void BaseElementPresenter::on_askUpdate()
{
    view()->update();
}

void BaseElementPresenter::selectAll()
{
    auto processmodel = model()->focusManager().focusedModel();
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
    if(path.vec().last().objectName() == "ConstraintModel"
    || path.vec().last().objectName() == "BaseConstraintModel")
    {
        model()->setDisplayedConstraint(&path.find<ConstraintModel>());
    }
}

void BaseElementPresenter::on_displayedConstraintChanged()
{
    m_scenarioPresenter->on_displayedConstraintChanged(displayedConstraint());

    // Set a new zoom ratio, such that the displayed constraint takes the whole screen.
    on_zoomSliderChanged(0);
    on_askUpdate();

    // Update the address bar
    view()->addressBar()->setTargetObject(IDocument::path(displayedConstraint()));
}

void BaseElementPresenter::setProgressBarTime(const TimeValue &t)
{
    m_progressBar->setPos({t.toPixels(m_millisecondsPerPixel), 0});
}

void BaseElementPresenter::setMillisPerPixel(ZoomRatio newFactor)
{
    // TODO harmonize
    m_millisecondsPerPixel = newFactor;
    m_mainTimeRuler->setPixelPerMillis(1.0 / m_millisecondsPerPixel);
    m_localTimeRuler->setPixelPerMillis(1.0 / m_millisecondsPerPixel);

    m_scenarioPresenter->on_zoomRatioChanged(m_millisecondsPerPixel);
}

void BaseElementPresenter::on_newSelection(const Selection& sel)
{
    int scroll = m_localTimeRuler->totalScroll();
    delete m_localTimeRuler;
    view()->newLocalTimeRuler();
    m_localTimeRuler = new LocalTimeRulerPresenter{view()->localTimeRuler(), this};
    m_localTimeRuler->scroll(scroll);
    m_localTimeRuler->setPixelPerMillis(1./m_millisecondsPerPixel);

    if (sel.empty())
    {
        m_localTimeRuler->setDuration(TimeValue::zero());
        m_localTimeRuler->setStartPoint(TimeValue::zero());
    }
    else
    {
        if(auto cstr = dynamic_cast<const ConstraintModel*>(*sel.begin()) )
        {
            m_localTimeRuler->setDuration(cstr->defaultDuration());
            m_localTimeRuler->setStartPoint(cstr->startDate());

            connect(cstr,               &ConstraintModel::defaultDurationChanged,
                    m_localTimeRuler,   &LocalTimeRulerPresenter::setDuration);
            connect(cstr,               &ConstraintModel::startDateChanged,
                    m_localTimeRuler,   &LocalTimeRulerPresenter::setStartPoint);
        }
    }
}

void BaseElementPresenter::on_zoomSliderChanged(double newzoom)
{
    // 1. Map from 0 - 1 to min - max for m_pixelsPerMillisecond.
    // Min: 90 pixels per ms
    // Default: 0.03 pixels per ms
    // Max: enough so that the whole base constraint fills the screen

    // mapZoom maps a value between 0 and 1 to the correct zoom.
    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    // computedMax : the number of pixels in a millisecond when the whole constraint
    // is displayed on screen;
    auto computedMax = [&] ()
    {
        // On veut que cette fonction retourne le facteur de
        // m_millisecondsPerPixel nécessaire pour que la contrainte affichée tienne à l'écran.
        double viewWidth = view()->view()->width();
        double duration =  displayedConstraint().defaultDuration().msec();

        return 20 + duration / viewWidth;
    };

    auto newMillisPerPix = mapZoom(1.0 - newzoom, 2., std::max(4., computedMax()));
    updateZoom(newMillisPerPix, QPointF(0,0));
}

void BaseElementPresenter::on_zoomOnWheelEvent(QPointF center, QPoint zoom)
{
    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    // computedMax : the number of pixels in a millisecond when the whole constraint
    // is displayed on screen;
    auto computedMax = [&] ()
    {
        // On veut que cette fonction retourne le facteur de
        // m_millisecondsPerPixel nécessaire pour que la contrainte affichée tienne à l'écran.
        double viewWidth = view()->view()->width();
        double duration =  displayedConstraint().defaultDuration().msec();

        return 5 + duration / viewWidth;
    };

    float zoomSpeed = 0.3;
    float zoomratio = (view()->zoomSlider()->value() + zoomSpeed * float(zoom.y())/float(view()->zoomSlider()->width()));

    if (zoomratio > 1.)
        zoomratio = 0.99;
    else if(zoomratio < 0.)
        zoomratio = 0.01;

    view()->zoomSlider()->setValue(zoomratio);

    auto newMillisPerPix = mapZoom(1.0 - zoomratio, 2., std::max(4., computedMax()));

    updateZoom(newMillisPerPix, center);

}

void BaseElementPresenter::on_viewSizeChanged(const QSize &s)
{
    m_progressBar->setHeight(s.height());
    on_zoomSliderChanged(view()->zoomSlider()->value());
}

void BaseElementPresenter::on_horizontalPositionChanged(int dx)
{
    m_mainTimeRuler->scroll(dx);
    m_localTimeRuler->scroll(dx);
}

void BaseElementPresenter::updateRect(const QRectF& rect)
{
    view()->view()->setSceneRect(rect);
    QRectF other{view()->rulerView()->sceneRect()};
    other.setWidth(rect.width());
    other.setX(rect.x());
    view()->rulerView()->setSceneRect(other);
}

void BaseElementPresenter::updateZoom(ZoomRatio newZoom, QPointF focus)
{
    auto w = view()->view()->viewport()->width();
    auto h = view()->view()->viewport()->height();

    QRect viewport_rect = view()->view()->viewport()->rect() ;
    QRectF visible_scene_rect = view()->view()->mapToScene(viewport_rect).boundingRect();


    qreal center = (focus.isNull() ?
                  visible_scene_rect.center().x() * m_millisecondsPerPixel :
                  focus.x() * m_millisecondsPerPixel);

    auto leftT = visible_scene_rect.left() * m_millisecondsPerPixel;
    auto deltaX = (center - leftT) / m_millisecondsPerPixel;

    auto y = visible_scene_rect.top();

    if(newZoom != m_millisecondsPerPixel)
        setMillisPerPixel(newZoom);

    qreal x;
        x = center/m_millisecondsPerPixel - deltaX;

    auto newView = QRectF{x, y,(qreal)w, (qreal)h};

    view()->view()->ensureVisible(newView,0,0);
}


