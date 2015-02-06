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

QString AutomationModel::processName() const
{
	return "Automation";
}

void AutomationModel::serialize(SerializationIdentifier identifier, void* data) const
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

ProcessViewModelInterface* AutomationModel::makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
														  QObject* parent)
{
	return new AutomationViewModel{this, viewModelId, parent};
}

ProcessViewModelInterface* AutomationModel::makeViewModel(SerializationIdentifier identifier,
														  void* data,
														  QObject* parent)
{
	qDebug() << Q_FUNC_INFO << "TODO";
	return nullptr;
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
