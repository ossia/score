#pragma once

namespace iscore
{
	// TODO better name, to prevent ambiguous "Panel".
	class DocumentPanelView;
	class DocumentPanelPresenter;
	class DocumentPanelModel;
	class DocumentPresenter;
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