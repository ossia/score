#pragma once
#include <interface/panel/PanelFactoryInterface.hpp>



class InspectorPanelFactory : public iscore::PanelFactoryInterface
{


		// PanelFactoryInterface interface
	public:
		virtual iscore::PanelViewInterface* makeView (iscore::View*);
		virtual iscore::PanelPresenterInterface* makePresenter (iscore::Presenter* parent_presenter,
		        iscore::PanelModelInterface* model,
		        iscore::PanelViewInterface* view);
		virtual iscore::PanelModelInterface* makeModel (iscore::Model*);
};
