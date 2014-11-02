#pragma once
#include <interface/docpanel/DocumentPanelModel.hpp>

class HelloWorldCentralPanelModel  : public iscore::DocumentPanelModel
{
	public:
		HelloWorldCentralPanelModel():
			iscore::DocumentPanelModel{nullptr}
		{
			
		}
		virtual ~HelloWorldCentralPanelModel() = default; 

		virtual void setPresenter(iscore::DocumentPanelPresenter* presenter) override;
};
