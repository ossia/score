#pragma once

class QWidget;
class Presenter;

namespace iscore
{
	class PanelPresenter;
	
	class PanelModel
	{
			
	};
	
	class PanelView
	{
		public:
			virtual ~PanelView() = default;
			virtual void setPresenter(PanelPresenter* presenter) = 0;
			virtual QWidget* getWidget() = 0; 
	};
	
	class PanelPresenter
	{
		public:
			PanelPresenter(Presenter* parent_presenter, PanelModel* model, PanelView* view):
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{
				
			}

			virtual ~PanelPresenter() = default;

		protected:
			PanelModel* m_model;
			PanelView* m_view;
			Presenter* m_parentPresenter;
	};
	
	class Panel
	{
		public:
			virtual PanelView* makeView(QString view) = 0;
			virtual PanelPresenter* makePresenter() = 0;
			virtual PanelModel* makeModel() = 0;
	};
}
