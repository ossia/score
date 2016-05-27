#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <iscore/widgets/DoubleSlider.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp>
#include <iscore/application/ApplicationContext.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <QtGlobal>
#include <QPolygon>
#include <QSize>
#include <QString>
#include <QWidget>

#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include "ScenarioDocumentPresenter.hpp"
#include "ZoomPolicy.hpp"
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <iscore/tools/ObjectIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/Todo.hpp>


namespace iscore {
class DocumentDelegateModelInterface;
class DocumentDelegateViewInterface;
class DocumentPresenter;
}  // namespace iscore

namespace Scenario
{
const ScenarioDocumentModel& ScenarioDocumentPresenter::model() const
{
    return static_cast<const ScenarioDocumentModel&>(m_model);
}

ZoomRatio ScenarioDocumentPresenter::zoomRatio() const
{
    return m_zoomRatio;
}

ScenarioDocumentView& ScenarioDocumentPresenter::view() const
{
    return safe_cast<ScenarioDocumentView&>(m_view);
}

ScenarioDocumentPresenter::ScenarioDocumentPresenter(
        iscore::DocumentPresenter* parent_presenter,
        const iscore::DocumentDelegateModelInterface& delegate_model,
        iscore::DocumentDelegateViewInterface& delegate_view) :
    DocumentDelegatePresenterInterface {parent_presenter,
                                        "ScenarioDocumentPresenter",
                                        delegate_model,
                                        delegate_view},
    m_scenarioPresenter{new DisplayedElementsPresenter{this}},
    m_selectionDispatcher{iscore::IDocument::documentContext(model()).selectionStack},
    m_context{iscore::IDocument::documentContext(model()), m_focusDispatcher},
    m_mainTimeRuler{new TimeRulerPresenter{view().timeRuler(), this}}
{
    using namespace iscore;

    // Setup the connections
    con((m_selectionDispatcher.stack()), &SelectionStack::currentSelectionChanged,
        this,                            &ScenarioDocumentPresenter::on_newSelection);
    con(view(), &ScenarioDocumentView::horizontalZoomChanged,
        this,   &ScenarioDocumentPresenter::on_zoomSliderChanged);
    con(view().view(), &ProcessGraphicsView::sizeChanged,
        this,          &ScenarioDocumentPresenter::on_viewSizeChanged);
    con(view().view(), &ProcessGraphicsView::zoom,
        this,          &ScenarioDocumentPresenter::on_zoomOnWheelEvent);
    con(view(), &ScenarioDocumentView::horizontalPositionChanged,
        this,   &ScenarioDocumentPresenter::on_horizontalPositionChanged);

    connect(this, &ScenarioDocumentPresenter::requestDisplayedConstraintChange,
            &model(), &ScenarioDocumentModel::setDisplayedConstraint);
    connect(m_scenarioPresenter, &DisplayedElementsPresenter::requestFocusedPresenterChange,
            &model().focusManager(), static_cast<void (Process::ProcessFocusManager::*)(QPointer<Process::LayerPresenter>)>(&Process::ProcessFocusManager::focus));

    con(model(), &ScenarioDocumentModel::focusMe,
        this,    [&] () { view().view().setFocus(); });

    connect(m_mainTimeRuler->view(), &TimeRulerView::drag,
            this, [&] (QPointF click, QPointF current) {
        on_zoomOnWheelEvent((current - click).toPoint(), current);
    });

    // Focus
    connect(&m_focusDispatcher, SIGNAL(focus(QPointer<Process::LayerPresenter>)),
            &model(), SIGNAL(setFocusedPresenter(QPointer<Process::LayerPresenter>)),
            Qt::QueuedConnection);

    // Show our constraint
    con(model(), &ScenarioDocumentModel::displayedConstraintChanged,
        this, &ScenarioDocumentPresenter::on_displayedConstraintChanged);

    emit requestDisplayedConstraintChange(model().baseConstraint());
}

const ConstraintModel& ScenarioDocumentPresenter::displayedConstraint() const
{
    return model().displayedElements.constraint();
}

void ScenarioDocumentPresenter::on_askUpdate()
{
    view().update();
}

void ScenarioDocumentPresenter::selectAll()
{
    auto processmodel = model().focusManager().focusedModel();
    if(processmodel)
    {
        m_selectionDispatcher.setAndCommit(processmodel->selectableChildren());
    }
}

void ScenarioDocumentPresenter::deselectAll()
{
    m_selectionDispatcher.setAndCommit(Selection{});
}

void ScenarioDocumentPresenter::on_displayedConstraintChanged()
{
    auto& cst = displayedConstraint();
    // Setup of the state machine.
    auto& ctx = iscore::IDocument::documentContext(model());
    const auto& fact = ctx.app.components.factory<DisplayedElementsToolPaletteFactoryList>();
    m_stateMachine = fact.make(&DisplayedElementsToolPaletteFactory::make, *this, cst);
    m_scenarioPresenter->on_displayedConstraintChanged(cst);
    connect(m_scenarioPresenter->constraintPresenter(), &FullViewConstraintPresenter::constraintSelected,
            this, &ScenarioDocumentPresenter::requestDisplayedConstraintChange);

    // Set a new zoom ratio, such that the displayed constraint takes the whole screen.

    ZoomRatio newZoom = displayedConstraint().fullView()->zoom();
    if(newZoom != -1) // constraint has already been in fullview
    {
        double newSliderPos = ZoomPolicy::zoomRatioToSliderPos(
                                  newZoom,
                                  displayedConstraint().duration.defaultDuration().msec(),
                                  view().view().width()
                                  );
        view().zoomSlider()->setValue(newSliderPos);
    }
    else // first time in fullview : init the zoom ratio
    {
        view().zoomSlider()->setValue(0.01);
        newZoom = ZoomPolicy::sliderPosToZoomRatio(
                      0.01,
                      displayedConstraint().duration.defaultDuration().msec(),
                      view().view().width()
                      );
    }

    setMillisPerPixel(newZoom);
    // scroll to the last center position

    view().view().centerOn(displayedConstraint().fullView()->center());

    on_askUpdate();
}

void ScenarioDocumentPresenter::setMillisPerPixel(ZoomRatio newRatio)
{
    m_zoomRatio = newRatio;

    m_mainTimeRuler->setPixelPerMillis(1.0 / m_zoomRatio);
    m_scenarioPresenter->on_zoomRatioChanged(m_zoomRatio);
}

void ScenarioDocumentPresenter::on_newSelection(const Selection& sel)
{
}

void ScenarioDocumentPresenter::on_zoomSliderChanged(double sliderPos)
{
    auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
                               sliderPos,
                               displayedConstraint().duration.defaultDuration().msec(),
                               view().view().width()
                               );

    updateZoom(newMillisPerPix, QPointF(0,0));
}

void ScenarioDocumentPresenter::on_zoomOnWheelEvent(QPoint zoom, QPointF center)
{
    // convert the mouse displacement into a fake slider move

    double zoomSpeed = 1.5; // experiment value
    double newSliderPos = (view().zoomSlider()->value() +
                           zoomSpeed * float(zoom.y())/float(view().zoomSlider()->width()));

    if (newSliderPos > 1.)
        newSliderPos = 0.99;
    else if(newSliderPos < 0.)
        newSliderPos = 0.01;

    view().zoomSlider()->setValue(newSliderPos);

    auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
                               newSliderPos,
                               displayedConstraint().duration.defaultDuration().msec(),
                               view().view().width()
                               );

    updateZoom(newMillisPerPix, center);

}

void ScenarioDocumentPresenter::on_viewSizeChanged(const QSize &s)
{
    auto zoom = ZoomPolicy::sliderPosToZoomRatio(
                    view().zoomSlider()->value(),
                    displayedConstraint().duration.defaultDuration().msec(),
                    view().view().width());

    m_mainTimeRuler->view()->setWidth(s.width());
    updateZoom(zoom, {0,0});

    // update the center of view
    displayedConstraint().fullView()->setCenter(view().view().mapToScene(view().view().viewport()->rect()).boundingRect().center());
}

void ScenarioDocumentPresenter::on_horizontalPositionChanged(int dx)
{
    QRect viewport_rect = view().view().viewport()->rect() ;
    QRectF visible_scene_rect = view().view().mapToScene(viewport_rect).boundingRect();

    m_mainTimeRuler->setStartPoint(TimeValue::fromMsecs(visible_scene_rect.x() * m_zoomRatio));
    displayedConstraint().fullView()->setCenter(visible_scene_rect.center());
}

void ScenarioDocumentPresenter::on_elementsScaleChanged(double s)
{
    ;
}

void ScenarioDocumentPresenter::updateRect(const QRectF& rect)
{
    view().view().setSceneRect(rect);
}

void ScenarioDocumentPresenter::updateZoom(ZoomRatio newZoom, QPointF focus)
{
    auto w = view().view().viewport()->width();
    auto h = view().view().viewport()->height();

    QRect viewport_rect = view().view().viewport()->rect() ;
    QRectF visible_scene_rect = view().view().mapToScene(viewport_rect).boundingRect();

    qreal center = focus.x();
    if (focus.isNull())
    {
           center = visible_scene_rect.center().x();
    }
    else if (focus.x() - visible_scene_rect.left() < 40)
    {
        center = visible_scene_rect.left();
    }
    else if (visible_scene_rect.right() - focus.x() < 40)
    {
        center = visible_scene_rect.right();
    }


    qreal centerT = center * m_zoomRatio; // here's the old zoom

    auto deltaX = center - visible_scene_rect.left();

    auto y = visible_scene_rect.top();

    if(newZoom != m_zoomRatio)
        setMillisPerPixel(newZoom);


    qreal x = centerT/m_zoomRatio - deltaX;; // here's the new zoom

    auto newView = QRectF{x, y,(qreal)w, (qreal)h};

    view().view().ensureVisible(newView,0,0);

    QRectF new_visible_scene_rect = view().view().mapToScene(view().view().viewport()->rect()).boundingRect();

    // TODO should call displayedElementsPresenter instead??
    displayedConstraint().fullView()->setZoom(m_zoomRatio);
    displayedConstraint().fullView()->setCenter(new_visible_scene_rect.center());
}
}
