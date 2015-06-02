#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"


#include "../Commands/AddPoint.hpp"
#include "../Commands/MovePoint.hpp"


#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <QGraphicsSceneMouseEvent>

#include "QCustomPlotProcess/QCustomPlotCurve.hpp"

#include "CurveTest/CurveModel.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveView.hpp"
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
    connect(&m_viewModel.model(), &AutomationModel::pointsChanged,
            this, &AutomationPresenter::on_modelPointsChanged);

    auto cv = new CurveView(m_view);
    m_cp = new CurvePresenter(&m_viewModel.model().curve(), cv);

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

QVector<QPointF> mapToVector(QMap<double, double> map)
{
    QVector<QPointF> list;
    for(auto key : map.keys())
    {
        list.push_back({key, 1.0 - map[key]});
    }

    return list;
}

void AutomationPresenter::on_modelPointsChanged()
{
    m_cp->setRect(m_view->boundingRect());
}

