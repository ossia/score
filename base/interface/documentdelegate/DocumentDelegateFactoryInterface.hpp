#pragma once

namespace iscore
{
	// TODO better name, to prevent ambiguous "Panel".
	class DocumentDelegateViewInterface;
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateModelInterface;
	class DocumentPresenter;

	/**
	 * @brief The DocumentPanel class
	 *
	 * Similar to panel, but for the main document (like MS Word's main page)
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
