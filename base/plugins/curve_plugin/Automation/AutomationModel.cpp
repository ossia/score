#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"

AutomationModel::AutomationModel(id_type<ProcessSharedModelInterface> id,
								 QObject* parent):
	ProcessSharedModelInterface{id, "Automation", parent}
{

	// Demo
	addPoint(0, 0);
	addPoint(1, 1);
}

ProcessSharedModelInterface* AutomationModel::clone(id_type<ProcessSharedModelInterface> newId,
													QObject *newParent)
{
	auto autom = new AutomationModel{newId, newParent};
	autom->setAddress(address());
	autom->setPoints(QMap<double, double>{points()});

	return autom;
}

QString AutomationModel::processName() const
{
	return "Automation";
}

void AutomationModel::setDurationWithScale(TimeValue newDuration)
{
	qDebug() << Q_FUNC_INFO << "Todo";

	m_scale = duration().msec() / double(newDuration.msec());
	setDuration(newDuration);

	// Note : the presenter must draw the points correctly, by taking
	// into account the seconds <-> pixel ratio, and the zoom.
}

void AutomationModel::setDurationWithoutScale(TimeValue newDuration)
{
	setDuration(newDuration);

	// If the duration increases further than the last point,
	// we create a new point at the same level than the previous.
	// If it decreases, nothing happens (the points are kept in memory).

	// Note : we have to keep the "max" duration (i.e. duration from first to
	// last point.
}

ProcessViewModelInterface* AutomationModel::makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
														  QObject* parent)
{
	return new AutomationViewModel{this, viewModelId, parent};
}

ProcessViewModelInterface* AutomationModel::makeViewModel(id_type<ProcessViewModelInterface> newId,
												 const ProcessViewModelInterface* source,
												 QObject* parent)
{
	return new AutomationViewModel{static_cast<const AutomationViewModel*>(source), this, newId, parent};
}

// Note : the presenter should see the modifications happening,
// and prevent accidental point removal.
void AutomationModel::addPoint(double x, double y)
{
	m_points[x] = y;
	emit pointsChanged();
}

void AutomationModel::removePoint(double x)
{
	m_points.remove(x);
	emit pointsChanged();
}

void AutomationModel::movePoint(double oldx, double newx, double newy)
{
	m_points.remove(oldx);

	m_points[newx] = newy;
	emit pointsChanged();
}
