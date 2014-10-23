#pragma once
#include <interface/panels/Panel.hpp>
class ScenarioCentralPanelPresenter;
class ScenarioCentralPanelView : public iscore::PanelView
{
	public:
		ScenarioCentralPanelView();
		virtual ~ScenarioCentralPanelView() = default;
		
		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		ScenarioCentralPanelPresenter* m_presenter;
};
