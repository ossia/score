#pragma once
#include <interface/panels/Panel.hpp>

class ScenarioPanelPresenter : public iscore::PanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::PanelPresenter::PanelPresenter;
		virtual ~ScenarioPanelPresenter() = default;
};
