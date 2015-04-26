#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"


#include "../Commands/AddPoint.hpp"
#include "../Commands/MovePoint.hpp"
// TODO #include "../Commands/RemovePoint.hpp"


#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <QGraphicsSceneMouseEvent>

#include "QCustomPlotProcess/QCustomPlotCurve.hpp"

AutomationPresenter::AutomationPresenter(ProcessViewModelInterface* model,
                                         ProcessViewInterface* view,
                                         QObject* parent) :
    ProcessPresenterInterface {"AutomationPresenter", parent},
    m_viewModel {static_cast<AutomationViewModel*>(model) },
    m_view {static_cast<AutomationView*>(view) },
    m_curve{new QCustomPlotCurve{m_view}},
    m_commandDispatcher{iscore::IDocument::documentFromObject(model->sharedProcessModel())->commandStack()},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel->sharedProcessModel())}
{
    connect(m_viewModel->model(), &AutomationModel::pointsChanged,
            this, &AutomationPresenter::on_modelPointsChanged);


    connect(m_curve, &QCustomPlotCurve::pointMovingFinished,
            [&](double oldx, double newx, double newy)
    {
        auto cmd = new MovePoint{iscore::IDocument::path(m_viewModel->model()),
                                 oldx,
                                 newx,
                                 1.0 - newy};

        m_commandDispatcher.submitCommand(cmd);
    });

    connect(m_curve, &QCustomPlotCurve::pointCreated,
            [&](QPointF pt)
    {
        auto cmd = new AddPoint{iscore::IDocument::path(m_viewModel->model()),
                   pt.x(),
                   1.0 - pt.y()};

        m_commandDispatcher.submitCommand(cmd);
    });

    connect(m_curve, &QCustomPlotCurve::mousePressed,
            this, [&] ()
    {
        m_focusDispatcher.focus(m_viewModel);
    });

    parentGeometryChanged();
    on_modelPointsChanged();
}

void AutomationPresenter::setWidth(int width)
{
    m_view->setWidth(width);
    m_curve->setSize({m_view->width(), m_view->height()});
}

void AutomationPresenter::setHeight(int height)
{
    m_view->setHeight(height);
    m_curve->setSize({m_view->width(), m_view->height()});
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

    m_curve->redraw();
    m_curve->setSize({m_view->width(), m_view->height()});
}

void AutomationPresenter::parentGeometryChanged()
{
    m_curve->setSize({m_view->width(), m_view->height()});
}

id_type<ProcessViewModelInterface> AutomationPresenter::viewModelId() const
{
    return m_viewModel->id();
}

id_type<ProcessSharedModelInterface> AutomationPresenter::modelId() const
{
    return m_viewModel->model()->id();
}

QList<QPointF> mapToList(QMap<double, double> map)
{
    QList<QPointF> list;
    for(auto key : map.keys())
    {
        list.push_back({key, 1.0 - map[key]});
    }

    return list;
}

void AutomationPresenter::on_modelPointsChanged()
{
    m_curve->setPoints(mapToList(m_viewModel->model()->points()));
    parentGeometryChanged();
}

