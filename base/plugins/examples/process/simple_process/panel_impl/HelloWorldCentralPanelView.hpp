#pragma once
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class HelloWorldCentralPanelPresenter;
class HelloWorldCentralPanelView : public iscore::DocumentDelegateViewInterface
{
	public:
		HelloWorldCentralPanelView();
		virtual ~HelloWorldCentralPanelView() = default;

		virtual void setPresenter(iscore::DocumentDelegatePresenterInterface* presenter) override;

		virtual QWidget*getWidget() override;

	private:
		HelloWorldCentralPanelPresenter* m_presenter;
};
