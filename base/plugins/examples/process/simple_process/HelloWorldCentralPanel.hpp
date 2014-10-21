#pragma once
#include <interface/panels/BasePanel.hpp>

class HelloWorldCentralPanel :  public iscore::BasePanel
{
		
		
		// Panel interface
	public:
		virtual iscore::PanelView*makeView() override
		{
		}
		virtual iscore::PanelPresenter*makePresenter(iscore::Presenter* parent_presenter, iscore::PanelModel* model, iscore::PanelView* view) override
		{
		}
		virtual iscore::PanelModel*makeModel() override
		{
		}
};
