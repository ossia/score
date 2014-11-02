#pragma once
#include <interface/docpanel/DocumentPanel.hpp>

class ScenarioCentralPanel :  public iscore::DocumentPanel
{
	public:
		virtual iscore::DocumentPanelView* makeView() override;
		virtual iscore::DocumentPanelPresenter* makePresenter(iscore::DocumentPresenter* parent_presenter, 
															  iscore::DocumentPanelModel* model, 
															  iscore::DocumentPanelView* view) override;
		virtual iscore::DocumentPanelModel* makeModel() override;
};
