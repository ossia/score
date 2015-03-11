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
/*
#include "../Process/PluginCurveModel.hpp"
#include "../Process/PluginCurveView.hpp"
#include "../Process/PluginCurvePresenter.hpp"
*/

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

    /*
    qDebug() << Q_FUNC_INFO << (void*) this;
    if(m_curveView)
    {
        m_view->scene()->removeItem(m_curveView);
        m_curveView->deleteLater();
    }

    if(m_curveModel)
        m_curveModel->deleteLater();
    if(m_curvePresenter)
        m_curvePresenter->deleteLater();

    m_curveModel = new PluginCurveModel {this};
    m_curveView = new PluginCurveView {m_view};
    connect(m_curveView, &PluginCurveView::mousePressed,
            this, [&] (QGraphicsSceneMouseEvent*)
    {
        m_focusDispatcher.focus(m_viewModel);
    });

    // Compute the scale
    auto duration = m_viewModel->model()->duration();
    auto width = m_view->parentItem()->boundingRect().width();

    double scale =  1.0 / duration.toPixels(width * m_zoomRatio);

    m_curvePresenter = new PluginCurvePresenter {scale,
                       m_curveModel,
                       m_curveView,
                       this};

    // Recreate the points in the model
    auto pts = m_viewModel->model()->points();
    auto keys = pts.keys();

    for(int i = 0; i < keys.size(); ++i)
    {
        double x = keys[i];

        if(i != 0 && i != keys.size() - 1)
            m_curvePresenter->addPoint(m_curvePresenter->map()->scaleToPaint({x, pts[x]}));
        else
            m_curvePresenter->addPoint(m_curvePresenter->map()->scaleToPaint({x, pts[x]}), MobilityMode::Vertical);
    }

    m_curvePresenter->setAllFlags(true);

    // Connect required signals and slots.
    connect(m_curvePresenter, &PluginCurvePresenter::notifyPointCreated,
            [&](QPointF pt)
    {
        auto cmd = new AddPoint{iscore::IDocument::path(m_viewModel->model()),
                   pt.x(),
                   pt.y()};

        m_commandDispatcher->submitCommand(cmd);
    });

    connect(m_curvePresenter, &PluginCurvePresenter::notifyPointMoved,
            [&](QPointF oldPt, QPointF newPt)
    {
        auto cmd = new MovePoint{iscore::IDocument::path(m_viewModel->model()),
                   oldPt.x(), newPt.x(), newPt.y()
    };

        m_commandDispatcher->submitCommand(cmd);
    });

    connect(m_curvePresenter, &PluginCurvePresenter::notifyPointDeleted,
            [&](QPointF pt)
    {
        auto cmd = new RemovePoint{iscore::IDocument::path(m_viewModel->model()),
                   pt.x()};

        m_commandDispatcher->submitCommand(cmd);
    });
    */
}

