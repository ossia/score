    #pragma once
#include <memory>
#include <tools/NamedObject.hpp>

#include <core/interface/selection/SelectionStack.hpp>
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
            CommandQueue* commandQueue()
            {
                return m_commandQueue.get();
            }

            void setPresenterDelegate(DocumentDelegatePresenterInterface* pres);
            DocumentDelegatePresenterInterface* presenterDelegate() const
            {
                return m_presenter;
            }

            SelectionStack& selectionStack()
            {
                return m_selection;
            }

        signals:
            void lock(QByteArray);
            void unlock(QByteArray);

        private:
            void lock_impl();
            void unlock_impl();

            std::unique_ptr<CommandQueue> m_commandQueue;
            SerializableCommand* m_ongoingCommand {};
            ObjectPath m_lockedObject;

            DocumentDelegatePresenterInterface* m_presenter {};
            DocumentView* m_view {};
            DocumentModel* m_model {};

            SelectionStack m_selection;
    };
}
