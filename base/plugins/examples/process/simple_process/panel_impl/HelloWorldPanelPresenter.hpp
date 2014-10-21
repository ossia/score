#pragma once
#include <interface/panels/Panel.hpp>

class HelloWorldPanelPresenter : public iscore::PanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::PanelPresenter::PanelPresenter;
		virtual ~HelloWorldPanelPresenter() = default;
};
