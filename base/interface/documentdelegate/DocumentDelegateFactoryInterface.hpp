#pragma once

namespace iscore
{
	class DocumentDelegateViewInterface;
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateModelInterface;
	class DocumentPresenter;

	/**
	 * @brief The DocumentDelegateFactoryInterface class
	 *
	 * The interface required to create a custom main document (like MS Word's main page)
	 */
	class DocumentDelegateFactoryInterface
	{
		public:
			virtual DocumentDelegateViewInterface* makeView() = 0;
			virtual DocumentDelegatePresenterInterface* makePresenter(DocumentPresenter* parent_presenter,
														  DocumentDelegateModelInterface* model,
														  DocumentDelegateViewInterface* view) = 0;
			virtual DocumentDelegateModelInterface* makeModel() = 0;
	};

}
