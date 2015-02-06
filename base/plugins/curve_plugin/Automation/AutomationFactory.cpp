#include "AutomationFactory.hpp"
#include "AutomationModel.hpp"
#include "AutomationView.hpp"
#include "AutomationPresenter.hpp"


ProcessSharedModelInterface* AutomationFactory::makeModel(id_type<ProcessSharedModelInterface> id,
														   QObject* parent)
{
	return new AutomationModel{id, parent};
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
