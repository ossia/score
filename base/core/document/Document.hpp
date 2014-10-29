#pragma once
#include <QNamedObject>

namespace iscore
{
	class DocumentModel;
	class DocumentPresenter;
	class DocumentView;

	class Document : public QObject
	{
			Q_OBJECT
		public:
			Document(QObject* parent, QWidget* parentview);
			DocumentPresenter* presenter() { return m_presenter; }
			DocumentView* view() { return m_view; }

			void newDocument();
			void reset();

		signals:
			void newDocument_start(); // Faire end si nécessaire

		private:
			DocumentModel* m_model;
			DocumentPresenter* m_presenter;
			DocumentView* m_view;
	};
}
