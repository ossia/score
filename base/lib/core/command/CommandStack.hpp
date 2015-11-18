#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <QObject>
#include <QStack>
#include <memory>


namespace iscore
{
    /**
     * @brief The CommandStack class
     *
     * Mostly equivalent to QUndoStack, but has added signals / slots.
     * They are used to send & receive the commands to the network, for instance.
     */
    class CommandStack final : public QObject
    {
            Q_OBJECT
            ISCORE_SERIALIZE_FRIENDS(CommandStack, DataStream)

            friend class CommandBackupFile;
        public:
            explicit CommandStack(QObject* parent = nullptr);
            ~CommandStack();

            // Allows blocking of undo and redo.
            void enableActions()
            {
                canUndoChanged(canUndo());
                canRedoChanged(canRedo());
            }

            void disableActions()
            {
                canUndoChanged(false);
                canRedoChanged(false);
            }

            bool canUndo() const
            {
                return !m_undoable.empty();
            }

            bool canRedo() const
            {
                return !m_redoable.empty();
            }

            QString undoText() const
            {
                return canUndo() ? m_undoable.top()->description() : tr("Nothing to undo");
            }

            QString redoText() const
            {
                return canRedo() ? m_redoable.top()->description() : tr("Nothing to redo");
            }


            int size() const
            {
                return m_undoable.size() + m_redoable.size();
            }

            const iscore::SerializableCommand* command(int index) const;
            int currentIndex() const
            {
                return m_undoable.size();
            }

            void markCurrentIndexAsSaved()
            {
                setSavedIndex(currentIndex());
            }

            bool isAtSavedIndex() const
            {
                return currentIndex() == m_savedIndex;
            }

        signals:
            /**
             * @brief push_start Is emitted when a command was pushed on the stack
             * @param cmd the command that was pushed
             */
            void localCommand(iscore::SerializableCommand* cmd);


            /**
             * @brief onUndo Is emitted when the user calls "Undo"
             */
            void localUndo();

            /**
             * @brief onRedo Is emitted when the user calls "Redo"
             */
            void localRedo();

            void canUndoChanged(bool);
            void canRedoChanged(bool);

            void undoTextChanged(QString);
            void redoTextChanged(QString);

            void indexChanged(int);
            void stackChanged();

            // These signals are low-level and are sent on each operation that affects the stacks
            void sig_undo();
            void sig_redo();
            void sig_push();
            void sig_indexChanged();

        public slots:
            void setIndex(int index);

            // These ones do not send signals
            void undoQuiet();
            void redoQuiet();

            /**
             * @brief push Pushes a command on the stack
             * @param cmd The command
             *
             * Calls cmd::redo()
             */
            void redoAndPush(iscore::SerializableCommand* cmd);


            /**
             * @brief quietPush Pushes a command on the stack
             * @param cmd The command
             *
             * Does NOT call cmd::redo()
             */
            void push(iscore::SerializableCommand* cmd);

            /**
             * @brief pushAndEmit Pushes a command on the stack and emit relevant signals
             * @param cmd The command
             */
            void redoAndPushQuiet(iscore::SerializableCommand* cmd);
            void pushQuiet(iscore::SerializableCommand* cmd);

            void undo()
            {
                undoQuiet();
                emit localUndo();
            }

            void redo()
            {
                redoQuiet();
                emit localRedo();
            }

        private:
            void setSavedIndex(int index);

            template<typename Callable>
            /**
             * @brief updateStack Updates the undo / redo stack
             * @param c A function object of prototype void(void)
             *
             * This function takes care of keeping everything synced
             * in the GUI.
             */
            void updateStack(Callable&& c)
            {
                bool pre_canUndo{canUndo()},
                     pre_canRedo{canRedo()};

                c();

                if(pre_canUndo != canUndo())
                    emit canUndoChanged(canUndo());

                if(pre_canRedo != canRedo())
                    emit canRedoChanged(canRedo());

                if(canUndo())
                    emit undoTextChanged(m_undoable.top()->description());
                else
                    emit undoTextChanged("");

                if(canRedo())
                    emit redoTextChanged(m_redoable.top()->description());
                else
                    emit redoTextChanged("");

                emit indexChanged(m_undoable.size() - 1);
                emit stackChanged();
            }

            QStack<iscore::SerializableCommand*> m_undoable;
            QStack<iscore::SerializableCommand*> m_redoable;

            int m_savedIndex{};
    };
}
