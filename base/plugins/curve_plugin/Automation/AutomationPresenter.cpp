#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <QGraphicsSceneMouseEvent>

AutomationPresenter::AutomationPresenter(ProcessViewModelInterface* model,
                                         ProcessViewInterface* view,
                                         QObject* parent) :
    ProcessPresenterInterface {"AutomationPresenter", parent},
    m_viewModel {static_cast<AutomationViewModel*>(model) },
    m_view {static_cast<AutomationView*>(view) },
    m_commandDispatcher{new CommandDispatcher<>{iscore::IDocument::documentFromObject(model->sharedProcessModel())->commandStack(), this}},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel->sharedProcessModel())}
{
    connect(m_viewModel->model(), &AutomationModel::pointsChanged,
            this, &AutomationPresenter::on_modelPointsChanged);

    on_modelPointsChanged();
}

void AutomationPresenter::setWidth(int width)
{
    m_view->setWidth(width);
}

void AutomationPresenter::setHeight(int height)
{
    m_view->setHeight(height);
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

id_type<ProcessViewModelInterface> AutomationPresenter::viewModelId() const
{
    return m_viewModel->id();
}

id_type<ProcessSharedModelInterface> AutomationPresenter::modelId() const
{
    return m_viewModel->model()->id();
}
#include "../AltProcess/Curve.hpp"

#include "../Commands/AddPoint.hpp"
#include "../Commands/MovePoint.hpp"
#include "../Commands/RemovePoint.hpp"

#include <ProcessInterface/ZoomHelper.hpp>

#include <QGraphicsScene>
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
    if(m_curve)
    {
        m_view->scene()->removeItem(m_curve);
        delete m_curve;
    }

    auto list = mapToList(m_viewModel->model()->points());
    m_curve = new Curve{list, m_view};
    m_curve->setPos(0, 0);
    m_curve->setZValue(15);
    m_curve->setSize({m_view->width(), m_view->height()});


    connect(m_curve, &Curve::pointMovingFinished,
            [&](double oldx, double newx, double newy)
    {
        auto cmd = new MovePoint{iscore::IDocument::path(m_viewModel->model()),
                                 oldx,
                                 newx,
                                 1.0 - newy};

        m_commandDispatcher->submitCommand(cmd);
    });


    connect(m_curve, &Curve::pointCreated,
            [&](QPointF pt)
    {
        auto cmd = new AddPoint{iscore::IDocument::path(m_viewModel->model()),
                   pt.x(),
                   1.0 - pt.y()};

        m_commandDispatcher->submitCommand(cmd);
    });


    connect(m_curve, &Curve::mousePressed,
            this, [&] ()
    {
        m_focusDispatcher.focus(m_viewModel);
    });
}

