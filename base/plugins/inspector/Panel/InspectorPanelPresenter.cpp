#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelView.hpp"

InspectorPanelPresenter::InspectorPanelPresenter (iscore::Presenter* parent, 
												  iscore::PanelModelInterface* model, 
												  iscore::PanelViewInterface* view) :
	iscore::PanelPresenterInterface {parent, model, view}
{
	auto i_model = static_cast<InspectorPanelModel*>(model);
	auto i_view  = static_cast<InspectorPanelView*>(view);
	
	connect(i_model, &InspectorPanelModel::setNewItem,
			i_view , &InspectorPanelView::on_setNewItem);
}
