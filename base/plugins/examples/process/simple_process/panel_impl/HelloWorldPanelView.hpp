#pragma once
#include <interface/panels/Panel.hpp>
class HelloWorldPanelPresenter;
class HelloWorldPanelView : public iscore::PanelView
{
	public:
		HelloWorldPanelView();
		virtual ~HelloWorldPanelView() = default;
		
		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		HelloWorldPanelPresenter* m_presenter;
};
