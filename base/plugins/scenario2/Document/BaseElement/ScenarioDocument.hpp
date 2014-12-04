#pragma once
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>

class ScenarioDocument : public iscore::DocumentDelegateFactoryInterface
{
	public:
		virtual iscore::DocumentDelegateViewInterface*makeView();
		virtual iscore::DocumentDelegatePresenterInterface*makePresenter(iscore::DocumentPresenter* parent_presenter, 
																		 iscore::DocumentDelegateModelInterface* model, 
																		 iscore::DocumentDelegateViewInterface* view);
		virtual iscore::DocumentDelegateModelInterface*makeModel();
};