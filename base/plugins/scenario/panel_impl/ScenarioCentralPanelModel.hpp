#pragma once
#include <interface/docpanel/DocumentPanelModel.hpp>

class ScenarioCentralPanelModel  : public iscore::DocumentPanelModel
{
	public:
		ScenarioCentralPanelModel():
			iscore::DocumentPanelModel{nullptr}
		{
			
		}
		virtual ~ScenarioCentralPanelModel() = default; 

		virtual void setPresenter(iscore::DocumentPanelPresenter* presenter) override;
};
