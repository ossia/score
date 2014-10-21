#pragma once
#include <QWidget>

namespace iscore
{
	class Presenter;	
	class PanelPresenter;
	class Command;
	class PanelModel;

	class PanelView : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~PanelView() = default;
			virtual void setPresenter(PanelPresenter* presenter) = 0;
			virtual QWidget* getWidget() = 0; 
	};
	
	class PanelPresenter : public QObject
	{
			Q_OBJECT
		public:
			PanelPresenter(Presenter* parent_presenter, PanelModel* model, PanelView* view):
				QObject{},
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{
				
			}

			virtual ~PanelPresenter() = default;
			
		signals:
			void submitCommand(Command* cmd);

		protected:
			PanelModel* m_model;
			PanelView* m_view;
			Presenter* m_parentPresenter;
	};
	
	class Panel
	{
		public:
			virtual PanelView* makeView() = 0;
			virtual PanelPresenter* makePresenter(Presenter* parent_presenter, PanelModel* model, PanelView* view) = 0;
			virtual PanelModel* makeModel() = 0;
	};
}
