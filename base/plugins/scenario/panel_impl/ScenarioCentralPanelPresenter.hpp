#pragma once
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class ScenarioCentralPanelPresenter : public iscore::DocumentDelegatePresenterInterface
{
		Q_OBJECT
	public:

		using iscore::DocumentDelegatePresenterInterface::DocumentDelegatePresenterInterface;
		virtual ~ScenarioCentralPanelPresenter() = default;
};
