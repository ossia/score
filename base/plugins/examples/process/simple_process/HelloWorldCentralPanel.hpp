#pragma once
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>

class HelloWorldCentralPanel :  public iscore::DocumentDelegateFactoryInterface
{
	public:
		virtual iscore::DocumentDelegateViewInterface* makeView() override;
		virtual iscore::DocumentDelegatePresenterInterface* makePresenter(iscore::DocumentPresenter* parent_presenter,
															  iscore::DocumentDelegateModelInterface* model,
															  iscore::DocumentDelegateViewInterface* view) override;
		virtual iscore::DocumentDelegateModelInterface* makeModel() override;
};
