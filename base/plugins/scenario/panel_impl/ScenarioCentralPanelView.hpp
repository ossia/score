#pragma once
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

namespace iscore
{
	class DocumentDelegatePresenterInterface;
}
class ScenarioCentralPanelPresenter;
class ScenarioCentralPanelView : public iscore::DocumentDelegateViewInterface
{
	public:
		ScenarioCentralPanelView();
		virtual ~ScenarioCentralPanelView() = default;

		virtual QWidget* getWidget() override;

	private:
		ScenarioCentralPanelPresenter* m_presenter;
};
