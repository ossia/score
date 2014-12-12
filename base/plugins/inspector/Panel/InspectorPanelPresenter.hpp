#pragma once

#include <interface/panel/PanelPresenterInterface.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
	public:
		InspectorPanelPresenter (iscore::Presenter* parent,
		                         iscore::PanelModelInterface* model,
		                         iscore::PanelViewInterface* view);
		
};
