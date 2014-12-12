#pragma once
#include <interface/panel/PanelFactoryInterface.hpp>

class HelloWorldPanel : public iscore::PanelFactoryInterface
{
	public:
		HelloWorldPanel():
			iscore::PanelFactoryInterface{}
		{

		}

		virtual ~HelloWorldPanel() = default;

		virtual iscore::PanelViewInterface* makeView() override;
		virtual iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
													  iscore::PanelModelInterface* model,
													  iscore::PanelViewInterface* view) override;
		virtual iscore::PanelModelInterface* makeModel() override;
};
