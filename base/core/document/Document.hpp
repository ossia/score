#pragma once
#include <QObject>

namespace iscore
{
	class DocumentModel;
	class DocumentPresenter;
	class DocumentView;

	class Document : public QObject
	{
		public:
			Document(QObject* parent);
			DocumentPresenter* presenter() { return m_presenter; }

		private:
			DocumentModel* m_model;
			DocumentPresenter* m_presenter;
			DocumentView* m_view;
	};
}
