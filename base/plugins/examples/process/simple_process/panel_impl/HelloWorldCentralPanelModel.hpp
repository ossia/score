#pragma once
#include <interface/panels/PanelModel.hpp>

class HelloWorldCentralPanelModel  : public iscore::PanelModel
{
	public:
		HelloWorldCentralPanelModel():
			iscore::PanelModel{nullptr}
		{
			
		}
		virtual ~HelloWorldCentralPanelModel() = default; 

		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
};
