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
            DocumentPresenter (DocumentModel*, DocumentView*, QObject* parent);
            CommandQueue* commandQueue()
            {
                return m_commandQueue.get();
            }

            void newDocument();
            void reset();
            void setPresenterDelegate (DocumentDelegatePresenterInterface* pres);
            DocumentDelegatePresenterInterface* presenterDelegate() const
            {
                return m_presenter;
            }

        signals:
            void on_elementSelected (QObject* element);
            void on_lastElementSelected();
            void lock (QByteArray);
            void unlock (QByteArray);

        public slots:
            // Command-relative slots
            /**
             * @brief applyCommand
             *
             * This slot is to be used by clients when they want to simply apply a command.
             * A notification is then sent by the CommandQueue.
             */
            void applyCommand (iscore::SerializableCommand*);

            /**
             * These slots :
             *   * initiateOngoingCommand,
             *   * continueOngoingCommand,
             *   * validateOngoingCommand,
             *   * undoOngoingCommand
             *
             * Are to be used when a Command takes multiple "steps" that must be
             * checked by the user, and do impact the model.
             * For instance, when resizing an element with the mouse, it is necessary to see
             * the effects of the transformation on the whole model. But it has to be applied only once.
             *
             * The locking is for the network implementation : the specified object will appear "locked"
             * to other users and they won't be able to modify it, in order to prevent conflicts.
             *
             * First, call initiateOngoingCommand with the initial Command (for instance
             * in a mousePressEvent). (Command::mergeWith() must work).
             * Then, keep making new Commands at each "change" (for instance, each mouseMoveEvent) and
             * apply them with continueOngoingCommand.
             *
             * When everything is done (e.g. mouseReleaseEvent), validateOngoingCommand is to be called.
             * If the user wants to cancel his command, for instance by pressing the "Escape" key,
             * call undoOngoingCommand()
             *
             * A signal will be sent in case of validateOngoingCommand, to propagate it to the network.
             *
             */
            void initiateOngoingCommand (iscore::SerializableCommand*, QObject* objectToLock);
            void continueOngoingCommand (iscore::SerializableCommand*);
            void rollbackOngoingCommand();
            void validateOngoingCommand();

        private:
            void lock_impl();
            void unlock_impl();

            std::unique_ptr<CommandQueue> m_commandQueue;
            SerializableCommand* m_ongoingCommand {};
            ObjectPath m_lockedObject;

            DocumentDelegatePresenterInterface* m_presenter {};
            DocumentView* m_view {};
            DocumentModel* m_model {};
    };
}
