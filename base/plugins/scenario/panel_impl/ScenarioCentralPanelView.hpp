#pragma once
#include <interface/docpanel/DocumentPanelView.hpp>
namespace iscore
{
	class DocumentPanelPresenter;	
}
class ScenarioCentralPanelPresenter;
class ScenarioCentralPanelView : public iscore::DocumentPanelView
{
	public:
		ScenarioCentralPanelView();
		virtual ~ScenarioCentralPanelView() = default;
		
		virtual void setPresenter(iscore::DocumentPanelPresenter* presenter) override;
		
		virtual QWidget*getWidget() override;
		
	private:
		ScenarioCentralPanelPresenter* m_presenter;
};
