#pragma once
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>

class ScenarioDocument : public iscore::DocumentDelegateFactoryInterface
{
	public:
		virtual iscore::DocumentDelegateViewInterface*
		makeView(iscore::DocumentView* parent) override;

		virtual iscore::DocumentDelegatePresenterInterface*
		makePresenter(iscore::DocumentPresenter* parent_presenter,
					  iscore::DocumentDelegateModelInterface* model,
					  iscore::DocumentDelegateViewInterface* view) override;

		virtual iscore::DocumentDelegateModelInterface*
		makeModel(iscore::DocumentModel* parent) override;
		virtual iscore::DocumentDelegateModelInterface*
		makeModel(iscore::DocumentModel* parent, const QByteArray& data) override;
};
