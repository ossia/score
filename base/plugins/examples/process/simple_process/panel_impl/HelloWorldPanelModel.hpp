#pragma once
#include <interface/panels/PanelModel.hpp>

class HelloWorldPanelModel  : public iscore::PanelModel
{
	public:
		HelloWorldPanelModel():
			iscore::PanelModel{nullptr}
		{
			
		}
		virtual ~HelloWorldPanelModel() = default; 

		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
};
