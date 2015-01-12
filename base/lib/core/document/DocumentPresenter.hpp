#pragma once
#include <memory>
#include <tools/NamedObject.hpp>

#include <core/presenter/command/CommandQueue.hpp>
#include <core/tools/ObjectPath.hpp>

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
			void lock(ObjectPath&);
			void unlock(ObjectPath&);

		private slots:
			void applyCommand(iscore::SerializableCommand*);

			void initiateOngoingCommand(iscore::SerializableCommand*, QObject* objectToLock);
			void continueOngoingCommand(iscore::SerializableCommand*);
			void undoOngoingCommand();
			void validateOngoingCommand();

		private:
			std::unique_ptr<CommandQueue> m_commandQueue;
			SerializableCommand* m_ongoingCommand{};
			ObjectPath m_lockedObject;

			DocumentDelegatePresenterInterface* m_presenter{};
			DocumentView* m_view{};
	};
}
