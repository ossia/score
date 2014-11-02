#pragma once
#include <QObject>
#include <core/document/DocumentPresenter.hpp>

namespace iscore
{
	class DocumentPresenter;
	class DocumentPanelModel;
	class DocumentPanelView;
	class Command;
	
	class DocumentPanelPresenter : public QObject
	{
			Q_OBJECT
		public:
			DocumentPanelPresenter(DocumentPresenter* parent_presenter, 
								   DocumentPanelModel* model, 
								   DocumentPanelView* view):
				QObject{parent_presenter},
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{
				
			}
			
			virtual ~DocumentPanelPresenter() = default;
			
		signals:
			void submitCommand(Command* cmd);
			
		protected:
			DocumentPanelModel* m_model;
			DocumentPanelView* m_view;
			DocumentPresenter* m_parentPresenter;
	};
}