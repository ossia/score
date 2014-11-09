#pragma once
#include <interface/panel/PanelModelInterface.hpp>

class HelloWorldPanelModel  : public iscore::PanelModelInterface
{
	public:
		HelloWorldPanelModel():
			iscore::PanelModelInterface{nullptr}
		{

		}
		virtual ~HelloWorldPanelModel() = default;

		virtual void setPresenter(iscore::PanelPresenterInterface* presenter) override;
};
