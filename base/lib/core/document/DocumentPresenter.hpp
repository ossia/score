#pragma once
#include <memory>
#include <tools/NamedObject.hpp>

#include <core/presenter/command/CommandQueue.hpp>

namespace iscore
{
	class DocumentModel;
	class DocumentView;
	class DocumentDelegatePresenterInterface;

	/**
	 * @brief The DocumentPresenter class holds the logic for the main document.
	 *
	 * Its main use is to manage the command queue, since we use the Command pattern,
	 * by taking the commands from the document view and applying them on the document model.
	 */
	class DocumentPresenter : public NamedObject
	{
			Q_OBJECT
		public:
			DocumentPresenter(DocumentModel*, DocumentView*, QObject* parent);
			CommandQueue* commandQueue() { return m_commandQueue.get(); }

			void newDocument();
			void reset();
			void setPresenterDelegate(DocumentDelegatePresenterInterface* pres);

		signals:
			void on_elementSelected(QObject* element);

		private slots:
			void applyCommand(SerializableCommand*);

		private:
			std::unique_ptr<CommandQueue> m_commandQueue;
			DocumentDelegatePresenterInterface* m_presenter{};

			DocumentView* m_view{};
	};
}
