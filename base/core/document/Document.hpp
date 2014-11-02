#pragma once
#include <QNamedObject>

namespace iscore
{
	class DocumentModel;
	class DocumentPresenter;
	class DocumentView;
	class DocumentPanel;

	/**
	 * @brief The Document class is the central part of the software.
	 *
	 * It is similar to the opened file in Word for instance, this is the 
	 * data on which i-score operates, further defined by the plugins.
	 */
	class Document : public QObject
	{
			Q_OBJECT
		public:
			Document(QObject* parent, QWidget* parentview);
			DocumentPresenter* presenter() { return m_presenter; }
			DocumentView* view() { return m_view; }

			void newDocument();
			void setBasePanel(DocumentPanel* p);
			void reset();

		signals:
			/**
			 * @brief newDocument_start
			 *
			 * This signal is emitted before a new document is created.
			 */
			void newDocument_start(); // TODO Faire end si nécessaire

		private:
			DocumentModel* m_model;
			DocumentPresenter* m_presenter;
			DocumentView* m_view;
	};
}
