#pragma once
#include <interface/panels/PanelModel.hpp>

class ScenarioPanelModel  : public iscore::PanelModel
{
	public:
		ScenarioPanelModel():
			iscore::PanelModel{nullptr}
		{
			
		}
		virtual ~ScenarioPanelModel() = default; 

		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
};
