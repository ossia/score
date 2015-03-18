#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/DoubleSlider.hpp"
#include "Document/BaseElement/Widgets/AddressBar.hpp"
#include "Document/BaseElement/Widgets/TimeRuler.hpp"
#include "ProcessInterface/ZoomHelper.hpp"
#include "Widgets/ProgressBar.hpp"

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"

#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QSlider>
#include <QGraphicsView>
#include <QGraphicsScene>

using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
                                           DocumentDelegateModelInterface* delegate_model,
                                           DocumentDelegateViewInterface* delegate_view) :
    DocumentDelegatePresenterInterface {parent_presenter,
                                        "BaseElementPresenter",
                                        delegate_model,
                                        delegate_view},
    m_selectionDispatcher{iscore::IDocument::documentFromObject(model())->selectionStack()},
    m_progressBar{new ProgressBar}
{
    connect(view()->addressBar(), &AddressBar::objectSelected,
            this,				  &BaseElementPresenter::setDisplayedObject);
    connect(view(), &BaseElementView::horizontalZoomChanged,
            this,	&BaseElementPresenter::on_zoomSliderChanged);

    // TODO same for height
    connect(view()->view(), &SizeNotifyingGraphicsView::sizeChanged,
            this, &BaseElementPresenter::on_viewSizeChanged);

    view()->scene()->addItem(m_progressBar);
    setProgressBarTime(std::chrono::milliseconds{0});

    view()->timeRuler()->setDuration(model()->constraintModel()->defaultDuration());
    setDisplayedConstraint(model()->constraintModel());

    // Use the default value in the slider.
    /*
    on_horizontalZoomChanged(m_horizontalZoomValue);
    */
}

ConstraintModel* BaseElementPresenter::displayedConstraint() const
{
    return m_displayedConstraint;
}

void BaseElementPresenter::on_askUpdate()
{
    view()->update();
}

void BaseElementPresenter::selectAll()
{
    if(model()->focusedViewModel())
    {
        m_selectionDispatcher.send(model()->focusedProcess()->selectableChildren());
    }
}

void BaseElementPresenter::deselectAll()
{
    m_selectionDispatcher.send({});
}

void BaseElementPresenter::setDisplayedObject(ObjectPath path)
{
    if(path.vec().last().objectName() == "ConstraintModel"
    || path.vec().last().objectName() == "BaseConstraintModel")
    {
        setDisplayedConstraint(path.find<ConstraintModel>());
    }
}

void BaseElementPresenter::setDisplayedConstraint(ConstraintModel* c)
{
    if(c && c != m_displayedConstraint)
    {
        m_displayedConstraint = c;
        on_displayedConstraintChanged();
    }
}

void BaseElementPresenter::on_displayedConstraintChanged()
{
    auto constraintViewModel = m_displayedConstraint->fullView();

    auto cstrView = new FullViewConstraintView {this->view()->baseObject() };

    delete m_displayedConstraintPresenter;
    m_displayedConstraintPresenter = new FullViewConstraintPresenter {constraintViewModel,
                                cstrView,
                                this};

    //m_displayedConstraintPresenter->on_zoomRatioChanged(m_horizontalZoomValue);
    on_askUpdate();

    connect(m_displayedConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            this,						&BaseElementPresenter::on_askUpdate);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { updateRect({0,
                                       0,
                                       m_displayedConstraint->defaultDuration().toPixels(m_millisecondsPerPixel),
                                       height()});} );

    model()->setDisplayedConstraint(m_displayedConstraintPresenter->model());
    // Update the address bar
    view()->addressBar()
          ->setTargetObject(IDocument::path(displayedConstraint()));

    view()->timeRuler()->setPos(- m_displayedConstraint->startDate().msec() / m_millisecondsPerPixel, 0);
}

void BaseElementPresenter::setProgressBarTime(TimeValue t)
{
    m_progressBar->setPos({t.toPixels(m_millisecondsPerPixel), 0});
}

void BaseElementPresenter::setMillisPerPixel(double newFactor)
{
    m_millisecondsPerPixel = newFactor;
    view()->timeRuler()->setPixelPerMillis(1/m_millisecondsPerPixel);
}

void BaseElementPresenter::on_zoomSliderChanged(double newzoom)
{
    // 1. Map from 0 - 1 to min - max for m_pixelsPerMillisecond.
    // Min: 90 pixels per ms
    // Default: 0.03 pixels per ms
    // Max: enough so that the whole base constraint fills the screen

    // mapZoom maps a value between 0 and 1 to the correctzoom.
    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    // computedMax : the number of pixels in a millisecond when the whole constraint
    // is displayed on screen;
    auto computedMax = [&] ()
    {
        // Durée d'une base contrainte : X s.
        // On veut que cette fonction retourne le facteur de
        // m_millisecondsPerPixel nécessaire pour que X s tienne à l'écran.
        double viewWidth = view()->view()->width();
        double duration =  model()->constraintModel()->defaultDuration().msec();

        return 5 + duration / viewWidth;
    };

    setMillisPerPixel(mapZoom(1.0 - newzoom, 1./90., computedMax()));

    // Maybe translate
    m_displayedConstraintPresenter->on_zoomRatioChanged(m_millisecondsPerPixel);

    view()->timeRuler()->setPixelPerMillis(1 / m_millisecondsPerPixel);
    view()->timeRuler()->setPos(- m_displayedConstraint->startDate().msec() / m_millisecondsPerPixel, 0);
}

#include <QDesktopWidget>
void BaseElementPresenter::on_viewSizeChanged(QSize s)
{
    m_progressBar->setHeight(s.height());
    on_zoomSliderChanged(view()->zoomSlider()->value());
}

void BaseElementPresenter::updateRect(QRectF rect)
{
    view()->view()->setSceneRect(rect);
}

BaseElementModel* BaseElementPresenter::model() const
{
    return static_cast<BaseElementModel*>(m_model);
}

double BaseElementPresenter::height() const
{
    return m_displayedConstraintPresenter->abstractConstraintView()->height();
}

BaseElementView* BaseElementPresenter::view() const
{
    return static_cast<BaseElementView*>(m_view);
}
