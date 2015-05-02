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

#include <ProcessInterface/ProcessViewModelInterface.hpp>

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"

#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
                                           DocumentDelegateModelInterface* delegate_model,
                                           DocumentDelegateViewInterface* delegate_view) :
    DocumentDelegatePresenterInterface {parent_presenter,
                                        "BaseElementPresenter",
                                        delegate_model,
                                        delegate_view},
    m_selectionDispatcher{iscore::IDocument::documentFromObject(model())->selectionStack()},
    m_progressBar{new ProgressBar},
    m_mainTimeRuler{new TimeRulerPresenter{view()->timeRuler(), this} },
    m_localTimeRuler { new LocalTimeRulerPresenter{view()->localTimeRuler(), this} }
{
    connect(&(m_selectionDispatcher.stack()),  &SelectionStack::currentSelectionChanged,
            this,   &BaseElementPresenter::on_newSelection);

    connect(view()->addressBar(), &AddressBar::objectSelected,
            this,				  &BaseElementPresenter::setDisplayedObject);
    connect(view(), &BaseElementView::horizontalZoomChanged,
            this,	&BaseElementPresenter::on_zoomSliderChanged);

    connect(view()->view(), &SizeNotifyingGraphicsView::sizeChanged,
            this, &BaseElementPresenter::on_viewSizeChanged);

    connect(view(),     &BaseElementView::horizontalPositionChanged,
            this,       &BaseElementPresenter::on_horizontalPositionChanged);

    view()->scene()->addItem(m_progressBar);
    setProgressBarTime(std::chrono::milliseconds{0});

    m_mainTimeRuler->setDuration(model()->constraintModel()->defaultDuration());
    m_localTimeRuler->setDuration(model()->constraintModel()->defaultDuration());

    setDisplayedConstraint(model()->constraintModel());

    connect(model(), &BaseElementModel::focusMe,
            this, [&] () { view()->view()->setFocus(); });
}

const ConstraintModel* BaseElementPresenter::displayedConstraint() const
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
        m_selectionDispatcher.setAndCommit(model()->focusedViewModel()->sharedProcessModel().selectableChildren());
    }
}

void BaseElementPresenter::deselectAll()
{
    m_selectionDispatcher.setAndCommit({});
}

void BaseElementPresenter::setDisplayedObject(ObjectPath path)
{
    if(path.vec().last().objectName() == "ConstraintModel"
    || path.vec().last().objectName() == "BaseConstraintModel")
    {
        setDisplayedConstraint(path.find<ConstraintModel>());
    }
}

void BaseElementPresenter::setDisplayedConstraint(const ConstraintModel* c)
{
    if(c && c != m_displayedConstraint)
    {
        m_displayedConstraint = c;
        on_displayedConstraintChanged();

        disconnect(m_fullViewConnection);
        if(c != model()->constraintModel())
        {
            m_fullViewConnection =
                    connect(c, &QObject::destroyed,
                            this, [&] () { setDisplayedConstraint(model()->constraintModel()); });
        }
    }
}

void BaseElementPresenter::on_displayedConstraintChanged()
{
    const auto& constraintViewModel = *m_displayedConstraint->fullView();

    delete m_displayedConstraintPresenter;
    m_displayedConstraintPresenter = new FullViewConstraintPresenter {constraintViewModel,
                                     this->view()->baseObject(),
                                     this};

    m_mainTimeRuler->setStartPoint(- m_displayedConstraintPresenter->model().startDate());
    m_localTimeRuler->setDuration(TimeValue{std::chrono::milliseconds(0)});
    m_localTimeRuler->setStartPoint(TimeValue{std::chrono::milliseconds(0)});

    // Set a new zoom ratio, such that the displayed constraint takes the whole screen.
    on_zoomSliderChanged(0);
    on_askUpdate();

    connect(m_displayedConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            this,					        &BaseElementPresenter::on_askUpdate);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { updateRect({0,
                                       0,
                                       m_displayedConstraint->defaultDuration().toPixels(m_millisecondsPerPixel),
                                       height()});} );

    model()->setDisplayedConstraint(&m_displayedConstraintPresenter->model());

    // Update the address bar
    view()->addressBar()
            ->setTargetObject(IDocument::path(displayedConstraint()));
}

void BaseElementPresenter::setProgressBarTime(TimeValue t)
{
    m_progressBar->setPos({t.toPixels(m_millisecondsPerPixel), 0});
}

void BaseElementPresenter::setMillisPerPixel(double newFactor)
{
    m_millisecondsPerPixel = newFactor;
    m_mainTimeRuler->setPixelPerMillis(1.0/newFactor);
    m_localTimeRuler->setPixelPerMillis(1.0/newFactor);
}

#include <QApplication>
void BaseElementPresenter::on_newSelection(Selection sel)
{
    int scroll = m_localTimeRuler->totalScroll();
    delete m_localTimeRuler;
    view()->newLocalTimeRuler();
    m_localTimeRuler = new LocalTimeRulerPresenter{view()->localTimeRuler(), this};
    m_localTimeRuler->scroll(scroll);
    m_localTimeRuler->setPixelPerMillis(1/m_millisecondsPerPixel);

    if (sel.isEmpty())
    {
        m_localTimeRuler->setDuration(TimeValue::zero());
        m_localTimeRuler->setStartPoint(TimeValue::zero());
    }
    else
    {
        if(auto cstr = dynamic_cast<const ConstraintModel*>(sel.at(0)) )
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
        double duration =  m_displayedConstraint->defaultDuration().msec();

        return 5 + duration / viewWidth;
    };

    setMillisPerPixel(mapZoom(1.0 - newzoom, 2., std::max(4., computedMax())));

    m_displayedConstraintPresenter->on_zoomRatioChanged(m_millisecondsPerPixel);
    updateGrid();
}

void BaseElementPresenter::on_viewSizeChanged(QSize s)
{
    m_progressBar->setHeight(s.height());
    on_zoomSliderChanged(view()->zoomSlider()->value());
}

void BaseElementPresenter::on_horizontalPositionChanged(int dx)
{
    m_mainTimeRuler->scroll(dx);
    m_localTimeRuler->scroll(dx);
}

void BaseElementPresenter::updateGrid()
{
    QPainterPath grid;
    double x = 0;
    const auto theHeight = height();
    const auto trWidth = m_mainTimeRuler->view()->width();
    const auto trSp = m_mainTimeRuler->view()->graduationSpacing();

    while (x < trWidth)
    {
        grid.addRect(x, 0, 1, theHeight);
        x += trSp;
    }

    view()->view()->setGrid(std::move(grid));
}

void BaseElementPresenter::updateRect(QRectF rect)
{
    view()->view()->setSceneRect(rect);
    QRectF other{view()->rulerView()->sceneRect()};
    other.setWidth(rect.width());
    other.setX(rect.x());
    view()->rulerView()->setSceneRect(other);
}

BaseElementModel* BaseElementPresenter::model() const
{
    return static_cast<BaseElementModel*>(m_model);
}

double BaseElementPresenter::height() const
{
    return m_displayedConstraintPresenter->view()->height();
}

BaseElementView* BaseElementPresenter::view() const
{
    return static_cast<BaseElementView*>(m_view);
}
