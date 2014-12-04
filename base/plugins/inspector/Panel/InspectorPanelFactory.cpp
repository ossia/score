#include "InspectorPanelFactory.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
//#include <Interval/objectinterval.hpp>
using namespace iscore;


iscore::PanelViewInterface* InspectorPanelFactory::makeView (iscore::View* parent)
{
	return new InspectorPanelView {parent};
}

iscore::PanelPresenterInterface* InspectorPanelFactory::makePresenter (
    iscore::Presenter* parent_presenter,
    iscore::PanelModelInterface* model,
    iscore::PanelViewInterface* view)
{
	return new InspectorPanelPresenter {parent_presenter, model, view};
}

iscore::PanelModelInterface* InspectorPanelFactory::makeModel (iscore::Model* parent)
{
	return new InspectorPanelModel {parent};
}

