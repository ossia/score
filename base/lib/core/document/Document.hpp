#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class DocumentModel;
	class DocumentPresenter;
	class DocumentView;
	class DocumentDelegateFactoryInterface;
	class PanelFactoryInterface;
	class PanelPresenterInterface;
	/**
	 * @brief The Document class is the central part of the software.
	 *
	 * It is similar to the opened file in Word for instance, this is the
	 * data on which i-score operates, further defined by the plugins.
	 */
	class Document : public NamedObject
	{
			Q_OBJECT
		public:
			Document(QWidget* parentview, QObject* parent);

			DocumentModel* model() { return m_model; }
			DocumentPresenter* presenter() { return m_presenter; }
			DocumentView* view() { return m_view; }

			void newDocument();
			void setDocumentPanel(DocumentDelegateFactoryInterface* p);

			void setupPanel(PanelPresenterInterface* pres,
							PanelFactoryInterface* factory);
			void reset();

			QByteArray save();

		signals:
			/**
			 * @brief newDocument_start
			 *
			 * This signal is emitted before a new document is created.
			 */
			void newDocument_start();

			void on_elementSelected(QObject*);

		public slots:
			void load(QByteArray data);

		private:
			void setupDocument();
			DocumentModel* m_model{};
			DocumentView* m_view{};
			DocumentPresenter* m_presenter{};

			DocumentDelegateFactoryInterface* m_currentDocumentType{};
	};

	Document* documentFromObject(QObject* obj);
}
