#pragma once
#include <interface/panel/PanelViewInterface.hpp>

class HelloWorldPanelPresenter;
class HelloWorldPanelView : public iscore::PanelViewInterface
{
	public:
		HelloWorldPanelView();
		virtual ~HelloWorldPanelView() = default;

		virtual void setPresenter(iscore::PanelPresenterInterface* presenter) override;

		virtual QWidget*getWidget() override;

	private:
		HelloWorldPanelPresenter* m_presenter;
};
