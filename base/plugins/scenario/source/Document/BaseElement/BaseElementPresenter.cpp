#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/AddressBar.hpp"
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
            this,	&BaseElementPresenter::on_horizontalZoomChanged);
    connect(view(), &BaseElementView::positionSliderChanged,
            this,	&BaseElementPresenter::on_positionSliderChanged);

    // TODO same for height
    connect(view()->view(), &SizeNotifyingGraphicsView::sizeChanged,
            this, &BaseElementPresenter::on_viewSizeChanged);

    view()->scene()->addItem(m_progressBar);
    m_progressBar->setPos(0, 0);
    m_progressBar->setHeight(200);

    setDisplayedConstraint(model()->constraintModel());

    // Use the default value in the slider.
    on_horizontalZoomChanged(m_horizontalZoomValue);
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

    m_displayedConstraintPresenter->on_zoomRatioChanged(m_horizontalZoomValue);
    on_askUpdate();

    connect(m_displayedConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            this,						&BaseElementPresenter::on_askUpdate);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { updateRect({(double)view()->positionSlider()->value(), 0, 1, height()});} );

    model()->setDisplayedConstraint(m_displayedConstraintPresenter->model());
    // Update the address bar
    view()->addressBar()
          ->setTargetObject(IDocument::path(displayedConstraint()));

    // Set the new minimum zoom. It should be set such that :
    // - when the x position is 0
    // - when the zoom is minimal (minZ)
    // - for the current viewport
    //  => the view can see 3% more than the base constraint

    // minSlider = viewportwidth * 100 * 0.97 / constraintDuration

    //view()->zoomSlider()->setMinimum(view()->view()->width() * 97.0 / model()->constraintModel()->defaultDuration().msec());

    on_horizontalZoomChanged(m_horizontalZoomValue);
}

void BaseElementPresenter::setProgressBarTime(TimeValue t)
{
    m_progressBar->setPos({t.toPixels(millisecondsPerPixel(m_horizontalZoomValue)), 0});
}

void BaseElementPresenter::on_horizontalZoomChanged(int newzoom)
{
    m_horizontalZoomValue = newzoom;

    // Maybe translate
    m_displayedConstraintPresenter->on_zoomRatioChanged(millisecondsPerPixel(m_horizontalZoomValue));

    // Change the min & max of position slider, & current value
    // If zoom = min, positionSliderMax = 0.
    // Else positionSliderMax is for the end at max.

    int val = view()->positionSlider()->value();
    auto newMax = model()->constraintModel()
                    ->defaultDuration().toPixels(
                        millisecondsPerPixel(view()->zoomSlider()->value()))
                  - view()->view()->width();
    view()->positionSlider()->setMaximum(newMax);

    if(val > newMax)
    {
        view()->positionSlider()->setValue(newMax);
        on_positionSliderChanged(newMax);
    }
}

void BaseElementPresenter::on_positionSliderChanged(int newPos)
{
    updateRect({(double)newPos, 0, 1, height()});
}

void BaseElementPresenter::on_viewSizeChanged(QSize s)
{
    m_progressBar->setHeight(s.height());
    int val = view()->zoomSlider()->value();
    int newMin = s.width() / model()->constraintModel()->defaultDuration().msec();
    view()->zoomSlider()->setMinimum(newMin);

    if(val < newMin)
    {
        view()->zoomSlider()->setValue(newMin);
        on_horizontalZoomChanged(newMin);
    }
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
