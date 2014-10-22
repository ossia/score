#pragma once
#include <interface/panels/Panel.hpp>
class HelloWorldCentralPanelPresenter;
class HelloWorldCentralPanelView : public iscore::PanelView
{
	public:
		HelloWorldCentralPanelView();
		virtual ~HelloWorldCentralPanelView() = default;
		
		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		HelloWorldCentralPanelPresenter* m_presenter;
};
