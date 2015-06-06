#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
AutomationPresenter::AutomationPresenter(
        const ProcessViewModel& model,
        ProcessView* view,
        QObject* parent) :
    ProcessPresenter {"AutomationPresenter", parent},
    m_viewModel{static_cast<const AutomationViewModel&>(model)},
    m_view{static_cast<AutomationView*>(view)},
    m_commandDispatcher{iscore::IDocument::documentFromObject(m_viewModel.sharedProcessModel())->commandStack()},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel.sharedProcessModel())}
{
    connect(&m_viewModel.model(), &AutomationModel::curveChanged,
            this, &AutomationPresenter::on_modelPointsChanged);

    auto cv = new CurveView(m_view);
    m_curvepresenter = new CurvePresenter(&m_viewModel.model().curve(), cv);

    parentGeometryChanged();
    on_modelPointsChanged();
}

AutomationPresenter::~AutomationPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

void AutomationPresenter::setWidth(int width)
{
    m_view->setWidth(width);
    on_modelPointsChanged();
}

void AutomationPresenter::setHeight(int height)
{
    m_view->setHeight(height);
    on_modelPointsChanged();
}

void AutomationPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
}

void AutomationPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
}

void AutomationPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;
    on_modelPointsChanged();
}

void AutomationPresenter::parentGeometryChanged()
{
    on_modelPointsChanged();
}

const ProcessViewModel& AutomationPresenter::viewModel() const
{
    return m_viewModel;
}

const id_type<ProcessModel>& AutomationPresenter::modelId() const
{
    return m_viewModel.model().id();
}

void AutomationPresenter::on_modelPointsChanged()
{
    // Compute the rect with the duration of the process.
    QRectF rect = m_view->boundingRect(); // for the height
    rect.setWidth(m_viewModel.model().duration().toPixels(m_zoomRatio));

    m_curvepresenter->setRect(rect);
}

