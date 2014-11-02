#pragma once
#include <interface/docpanel/DocumentPanelPresenter.hpp>

class ScenarioCentralPanelPresenter : public iscore::DocumentPanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::DocumentPanelPresenter::DocumentPanelPresenter;
		virtual ~ScenarioCentralPanelPresenter() = default;
};
