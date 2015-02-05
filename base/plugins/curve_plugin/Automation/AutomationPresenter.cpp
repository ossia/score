#include "AutomationPresenter.hpp"
#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationView.hpp"

constexpr QPointF modelToSceneCoordinates(QPointF pt, const QRectF& rect)
{
	return {pt.x() * rect.width() + 1,
			0.5  * rect.height() * (1 - pt.y())};
}

constexpr QPointF sceneToModelCoordinates(QPointF pt, const QRectF& rect)
{
	return {(pt.x() - 1.0) / rect.width(),
			1.0 - pt.y() / (0.5 * rect.height())};
}

AutomationPresenter::AutomationPresenter(ProcessViewModelInterface* model,
										 ProcessViewInterface* view,
										 QObject* parent):
	ProcessPresenterInterface{"AutomationPresenter", parent},
	m_viewModel{static_cast<AutomationViewModel*>(model)},
	m_view{static_cast<AutomationView*>(view)}
{
	connect(m_viewModel->model(), &AutomationModel::pointsChanged,
			this, &AutomationPresenter::on_modelPointsChanged);

	on_modelPointsChanged();
}

void AutomationPresenter::putToFront()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
}

void AutomationPresenter::putBack()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
}

void AutomationPresenter::on_horizontalZoomChanged(int)
{
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

void AutomationPresenter::on_modelPointsChanged()
{
	delete m_curveModel;
	delete m_curveView;
	delete m_curvePresenter;

	m_curveModel = new PluginCurveModel{this};
	m_curveView = new PluginCurveView{m_view};
	m_curvePresenter = new PluginCurvePresenter{m_curveModel, m_curveView, this};

	// Recreate the points in the model
	auto pts = m_viewModel->model()->points();

	for(double x : pts.keys())
	{
		m_curvePresenter->addPoint(m_curvePresenter->map()->scaleToPaint({x, pts[x]}));
	}

	// Connect required signals and slots.
	connect(m_curvePresenter, &PluginCurvePresenter::notifyPointCreated,
			[&] (QPointF pt)
	{
		qDebug() << "Add point:" << pt;
		auto cmd = new AddPoint{
					ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
					pt.x(), pt.y()};

		submitCommand(cmd);
	});

	connect(m_curvePresenter, &PluginCurvePresenter::notifyPointMoved,
			[&] (QPointF oldPt, QPointF newPt)
	{
		qDebug() << "Move point:" << oldPt << newPt;
		auto cmd = new MovePoint{
				   ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
				   oldPt.x(), newPt.x(), newPt.y()};

		submitCommand(cmd);
	});

	connect(m_curvePresenter, &PluginCurvePresenter::notifyPointDeleted,
			[&] (QPointF pt)
	{
		qDebug() << "Remove point:" << pt;
		auto cmd = new RemovePoint{
				   ObjectPath::pathFromObject("BaseElementModel", m_viewModel->model()),
				   pt.x()};

		submitCommand(cmd);
	});
}
