#include "AutomationFactory.hpp"
#include "AutomationModel.hpp"
#include "AutomationView.hpp"
#include "AutomationPresenter.hpp"


ProcessSharedModelInterface* AutomationFactory::makeModel(id_type<ProcessSharedModelInterface> id,
														   QObject* parent)
{
	return new AutomationModel{id, parent};
}

ProcessSharedModelInterface*AutomationFactory::makeModel(SerializationIdentifier identifier,
														  void* data,
														  QObject* parent)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
	return nullptr;
}


ProcessViewInterface* AutomationFactory::makeView(QString view, QObject* parent)
{
	return new AutomationView{static_cast<QGraphicsObject*>(parent)};
}


ProcessPresenterInterface* AutomationFactory::makePresenter(ProcessViewModelInterface* viewModel,
															 ProcessViewInterface* view,
															 QObject* parent)
{
	return new AutomationPresenter{viewModel, view, parent};
}
