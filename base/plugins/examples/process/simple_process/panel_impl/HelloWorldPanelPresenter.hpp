#pragma once
#include <interface/panel/PanelPresenterInterface.hpp>

class HelloWorldPanelPresenter : public iscore::PanelPresenterInterface
{
		Q_OBJECT
	public:

		using iscore::PanelPresenterInterface::PanelPresenterInterface;
		virtual ~HelloWorldPanelPresenter() = default;
};
