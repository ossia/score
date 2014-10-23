#pragma once
#include <interface/panels/Panel.hpp>
class ScenarioPanelPresenter;
class ScenarioPanelView : public iscore::PanelView
{
	public:
		ScenarioPanelView();
		virtual ~ScenarioPanelView() = default;
		
		virtual void setPresenter(iscore::PanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		ScenarioPanelPresenter* m_presenter;
};
