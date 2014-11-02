#pragma once
#include <interface/docpanel/DocumentPanelView.hpp>

class HelloWorldCentralPanelPresenter;
class HelloWorldCentralPanelView : public iscore::DocumentPanelView
{
	public:
		HelloWorldCentralPanelView();
		virtual ~HelloWorldCentralPanelView() = default;
		
		virtual void setPresenter(iscore::DocumentPanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		HelloWorldCentralPanelPresenter* m_presenter;
};
