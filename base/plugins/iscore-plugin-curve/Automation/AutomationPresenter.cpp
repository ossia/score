#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationLayerModel.hpp"
#include "AutomationView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <core/document/Document.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
AutomationPresenter::AutomationPresenter(
        const LayerModel& model,
        LayerView* view,
        QObject* parent) :
    LayerPresenter {"AutomationPresenter", parent},
    m_viewModel{static_cast<const AutomationLayerModel&>(model)},
    m_view{static_cast<AutomationView*>(view)},
    m_commandDispatcher{iscore::IDocument::commandStack(m_viewModel.processModel())},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel.processModel())}
{
    con(m_viewModel.model(), &AutomationModel::curveChanged,
            this, &AutomationPresenter::updateCurve);

    auto cv = new CurveView{m_view};
    m_curvepresenter = new CurvePresenter{m_viewModel.model().curve(), cv, this};


    connect(cv, &CurveView::pressed,
            this, [&] (const QPointF&)
    {
        m_focusDispatcher.focus(this);
    });

    parentGeometryChanged();
    updateCurve();
}

AutomationPresenter::~AutomationPresenter()
{
    deleteGraphicsObject(m_view);
}

void AutomationPresenter::setWidth(int width)
{
    m_view->setWidth(width);
    updateCurve();
}

void AutomationPresenter::setHeight(int height)
{
    m_view->setHeight(height);
    updateCurve();
}

void AutomationPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    m_curvepresenter->enable();
}

void AutomationPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    m_curvepresenter->disable();
}

void AutomationPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;
    updateCurve();
}

void AutomationPresenter::parentGeometryChanged()
{
    updateCurve();
}

const LayerModel& AutomationPresenter::layerModel() const
{
    return m_viewModel;
}

const Id<Process>& AutomationPresenter::modelId() const
{
    return m_viewModel.model().id();
}

void AutomationPresenter::updateCurve()
{
    // Compute the rect with the duration of the process.
    QRectF rect = m_view->boundingRect(); // for the height
    rect.setWidth(m_viewModel.model().duration().toPixels(m_zoomRatio));

    m_curvepresenter->setRect(rect);
}

void AutomationPresenter::on_focusChanged()
{
    m_curvepresenter->enableActions(focused());
}
