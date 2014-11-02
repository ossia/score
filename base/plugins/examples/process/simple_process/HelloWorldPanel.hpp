#pragma once
#include <interface/panels/Panel.hpp>

class HelloWorldPanel : public iscore::Panel
{
	public:
		HelloWorldPanel(): 
			iscore::Panel{}
		{
			
		}
		
		virtual ~HelloWorldPanel() = default;

		virtual iscore::PanelView* makeView() override;
		virtual iscore::PanelPresenter* makePresenter(iscore::Presenter* parent_presenter, 
													  iscore::PanelModel* model, 
													  iscore::PanelView* view) override;
		virtual iscore::PanelModel* makeModel() override;
};
