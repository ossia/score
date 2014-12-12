#pragma once
#include <QObject>
#include <core/presenter/Presenter.hpp>
namespace iscore
{
	class PanelModelInterface;
	class PanelViewInterface;
	class SerializableCommand;

	class PanelPresenterInterface : public QObject
	{
			Q_OBJECT
		public:
			PanelPresenterInterface(Presenter* parent_presenter, PanelModelInterface* model, PanelViewInterface* view):
				QObject{parent_presenter},
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{

			}

			virtual ~PanelPresenterInterface() = default;

		signals:
			void submitCommand(SerializableCommand* cmd);

		protected:
			PanelModelInterface* m_model;
			PanelViewInterface* m_view;
			Presenter* m_parentPresenter;
	};
}
