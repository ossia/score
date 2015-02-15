#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"

AutomationPresenter::AutomationPresenter(ProcessViewModelInterface* model,
										 ProcessViewInterface* view,
										 QObject* parent):
	ProcessPresenterInterface{"AutomationPresenter", parent},
	m_viewModel{static_cast<AutomationViewModel*>(model)},
	m_view{static_cast<AutomationView*>(view)}
{
	connect(m_viewModel->model(), &AutomationModel::pointsChanged,
			this, &AutomationPresenter::on_modelPointsChanged, Qt::QueuedConnection);

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

void AutomationPresenter::putBack()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
}

void AutomationPresenter::on_horizontalZoomChanged(int val)
{
	m_zoomLevel = val;
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

#include "../Process/PluginCurveModel.hpp"
#include "../Process/PluginCurveView.hpp"
#include "../Process/PluginCurvePresenter.hpp"

#include "../Commands/AddPoint.hpp"
#include "../Commands/MovePoint.hpp"
#include "../Commands/RemovePoint.hpp"

#include <ProcessInterface/ZoomHelper.hpp>

#include <QGraphicsScene>
void AutomationPresenter::on_modelPointsChanged()
{
	if(m_curveView)
	{
		m_view->scene()->removeItem(m_curveView);
		m_curveView->deleteLater();
	}
	m_curveModel->deleteLater();
	m_curvePresenter->deleteLater();

	m_curveModel = new PluginCurveModel{this};
	m_curveView = new PluginCurveView{m_view};

	// Compute the scale
	auto mspp = millisecondsPerPixel(m_zoomLevel);
	auto duration = m_viewModel->model()->duration();
	auto width = m_view->parentItem()->boundingRect().width();

	// When duration.msec() == parentDuration.msec(), scale = 1;
	// Parent duration = width * mspp
	// scale = duration.msec() / parentduration.msec()
	double scale = (width * mspp) / duration.msec();

	m_curvePresenter = new PluginCurvePresenter{scale,
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
			[&] (QPointF pt)
	{
		auto cmd = new AddPoint{
				ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
				pt.x(), pt.y()};

		submitCommand(cmd);
	});

	connect(m_curvePresenter, &PluginCurvePresenter::notifyPointMoved,
			[&] (QPointF oldPt, QPointF newPt)
	{
		auto cmd = new MovePoint{
				   ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
				   oldPt.x(), newPt.x(), newPt.y()};

		submitCommand(cmd);
	});

	connect(m_curvePresenter, &PluginCurvePresenter::notifyPointDeleted,
			[&] (QPointF pt)
	{
		auto cmd = new RemovePoint{
				   ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
				   pt.x()};

		submitCommand(cmd);
	});
}

