#pragma once
#include <interface/panels/PanelModel.hpp>

class ScenarioCentralPanelModel  : public iscore::PanelModel
{
	public:
		ScenarioCentralPanelModel():
			iscore::PanelModel{nullptr}
		{
			
		}
		virtual ~ScenarioCentralPanelModel() = default; 

		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
};
