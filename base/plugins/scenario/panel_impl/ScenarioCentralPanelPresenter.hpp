#pragma once
#include <interface/panels/Panel.hpp>

class ScenarioCentralPanelPresenter : public iscore::PanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::PanelPresenter::PanelPresenter;
		virtual ~ScenarioCentralPanelPresenter() = default;
};
