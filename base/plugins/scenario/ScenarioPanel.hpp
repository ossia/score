#pragma once
#include <interface/panels/Panel.hpp>

class ScenarioPanel : public iscore::Panel
{
	public:
		ScenarioPanel(): 
			iscore::Panel{}
		{
			
		}
		
		virtual ~ScenarioPanel() = default;

		virtual iscore::PanelView* makeView() override;
		virtual iscore::PanelPresenter* makePresenter(iscore::Presenter* parent_presenter, 
													  iscore::PanelModel* model, 
													  iscore::PanelView* view) override;
		virtual iscore::PanelModel* makeModel() override;
};
