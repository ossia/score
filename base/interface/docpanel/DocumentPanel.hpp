#pragma once

namespace iscore
{
	// TODO better name, to prevent ambiguous "Panel".
	class DocumentPanelView;
	class DocumentPanelPresenter;
	class DocumentPanelModel;
	class DocumentPresenter;
	
	/**
	 * @brief The DocumentPanel class
	 * 
	 * Similar to panel, but for the main document (like MS Word's main page)
	 */
	class DocumentPanel
	{
		public:
			virtual DocumentPanelView* makeView() = 0;
			virtual DocumentPanelPresenter* makePresenter(DocumentPresenter* parent_presenter, 
														  DocumentPanelModel* model, 
														  DocumentPanelView* view) = 0;
			virtual DocumentPanelModel* makeModel() = 0;
	};
	
}