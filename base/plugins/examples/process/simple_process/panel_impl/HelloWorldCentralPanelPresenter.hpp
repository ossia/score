#pragma once
#include <interface/docpanel/DocumentPanelPresenter.hpp>

class HelloWorldCentralPanelPresenter : public iscore::DocumentPanelPresenter
{
		Q_OBJECT
	public:
		
		using iscore::DocumentPanelPresenter::DocumentPanelPresenter;
		virtual ~HelloWorldCentralPanelPresenter() = default;
};
