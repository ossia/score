#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/AddressBar.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"

#include "core/interface/document/DocumentInterface.hpp"
#include <QSlider>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QApplication>

using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
                                           DocumentDelegateModelInterface* delegate_model,
                                           DocumentDelegateViewInterface* delegate_view) :
    DocumentDelegatePresenterInterface {parent_presenter,
                                        "BaseElementPresenter",
                                        delegate_model,
                                        delegate_view
                                        }
{
    connect(view()->addressBar(), &AddressBar::objectSelected,
            this,				  &BaseElementPresenter::setDisplayedObject);
    connect(view(), &BaseElementView::horizontalZoomChanged,
            this,	&BaseElementPresenter::on_horizontalZoomChanged);
    connect(view(), &BaseElementView::positionSliderChanged,
            this,	&BaseElementPresenter::on_positionSliderChanged);

    connect(view()->view(), &ScoreGraphicsView::widthChanged,
            this,           &BaseElementPresenter::on_viewWidthChanged);


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
    if(model()->focusedProcess())
    {
        emit newSelection(model()->focusedProcess()->selectableChildren());
    }
}

void BaseElementPresenter::deselectAll()
{
    emit newSelection({});
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

    delete m_baseConstraintPresenter;
    m_baseConstraintPresenter = new FullViewConstraintPresenter {constraintViewModel,
                                cstrView,
                                this};


    m_baseConstraintPresenter->on_horizontalZoomChanged(m_horizontalZoomValue);
    on_askUpdate();

    connect(m_baseConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            this,						&BaseElementPresenter::on_askUpdate);

    connect(m_baseConstraintPresenter, &FullViewConstraintPresenter::newSelection,
            this,                      &BaseElementPresenter::newSelection);


    // Update the address bar
    view()->addressBar()
          ->setTargetObject(IDocument::path(displayedConstraint()));

    // Set the new minimum zoom. It should be set such that :
    // - when the x position is 0
    // - when the zoom is minimal (minZ)
    // - for the current viewport
    //  => the view can see 3% more than the base constraint

    // minSlider = viewportwidth * 100 * 0.97 / constraintDuration

    view()->zoomSlider()->setMinimum(view()->view()->width() * 97.0 / model()->constraintModel()->defaultDuration().msec());
}

void BaseElementPresenter::on_horizontalZoomChanged(int newzoom)
{
    m_horizontalZoomValue = newzoom;

    // Maybe translate
    m_baseConstraintPresenter->on_horizontalZoomChanged(m_horizontalZoomValue);

    // Change the min & max of position slider, & current value
    // If zoom = min, positionSliderMax = 0.
    // Else positionSliderMax is such that max = 3% more.

    int val = view()->positionSlider()->value();
    auto newMax = model()->constraintModel()->defaultDuration().msec() / millisecondsPerPixel(view()->zoomSlider()->value())
                  - 0.97 * view()->view()->width();
    view()->positionSlider()->setMaximum(newMax);

    if(val > newMax)
    {
        view()->positionSlider()->setValue(newMax);
        on_positionSliderChanged(newMax);
    }

}

void BaseElementPresenter::on_positionSliderChanged(int newPos)
{
    view()->view()->setSceneRect(newPos, 0, 1, 1);
}

void BaseElementPresenter::on_viewWidthChanged(int w)
{
    int val = view()->zoomSlider()->value();
    int newMin = w * 97.0 / model()->constraintModel()->defaultDuration().msec();
    view()->zoomSlider()->setMinimum(newMin);

    if(val < newMin)
    {
        view()->zoomSlider()->setValue(newMin);
        on_horizontalZoomChanged(newMin);
    }
}

BaseElementModel* BaseElementPresenter::model() const
{
    return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view() const
{
    return static_cast<BaseElementView*>(m_view);
}
