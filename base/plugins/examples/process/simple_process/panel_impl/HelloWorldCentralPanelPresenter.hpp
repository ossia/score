#pragma once
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class HelloWorldCentralPanelPresenter : public iscore::DocumentDelegatePresenterInterface
{
		Q_OBJECT
	public:

		using iscore::DocumentDelegatePresenterInterface::DocumentDelegatePresenterInterface;
		virtual ~HelloWorldCentralPanelPresenter() = default;
};
