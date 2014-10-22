#pragma once
#include <interface/panels/Panel.hpp>

class HelloWorldCentralPanelPresenter : public iscore::PanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::PanelPresenter::PanelPresenter;
		virtual ~HelloWorldCentralPanelPresenter() = default;
};
