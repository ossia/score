#pragma once
#include <QStack>
#include <public_interface/command/SerializableCommand.hpp>
#include <memory>


namespace iscore
{
    /**
     * @brief The CommandQueue class
     *
     * Mostly equivalent to QUndoStack, but has added signals / slots.
     * They are used to send & receive the commands to the network, for instance.
     */
    class CommandStack : public QObject
    {
            Q_OBJECT
        public:
            CommandStack(QObject* parent = nullptr);

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
                return canUndo() ? m_undoable.top()->text() : tr("Nothing to undo");
            }

            QString redoText() const
            {
                return canRedo() ? m_redoable.top()->text() : tr("Nothing to redo");
            }


            int size() const
            {
                return m_undoable.size() + m_redoable.size();
            }

            const iscore::SerializableCommand* command(int index) const;
            void setIndex(int index);
            int currentIndex()
            {
                return m_undoable.size();
            }

        signals:
            /**
             * @brief push_start Is emitted when a command was pushed on the stack
             * @param cmd the command that was pushed
             */
            void push_start(iscore::SerializableCommand* cmd);


            /**
             * @brief onUndo Is emitted when the user calls "Undo"
             */
            void onUndo();

            /**
             * @brief onRedo Is emitted when the user calls "Redo"
             */
            void onRedo();

            void canUndoChanged(bool);
            void canRedoChanged(bool);

            void undoTextChanged(QString);
            void redoTextChanged(QString);

            void indexChanged(int);
            void stackChanged();

        public slots:
            void undo();
            void redo();

            /**
             * @brief push Pushes a command on the stack
             * @param cmd The command
             *
             * Calls cmd::redo()
             */
            void push(iscore::SerializableCommand* cmd);


            /**
             * @brief quietPush Pushes a command on the stack
             * @param cmd The command
             *
             * Does NOT call cmd::redo()
             */
            void quietPush(iscore::SerializableCommand* cmd);

            /**
             * @brief pushAndEmit Pushes a command on the stack and emit relevant signals
             * @param cmd The command
             */
            void pushAndEmit(iscore::SerializableCommand* cmd);

            void undoAndNotify()
            {
                undo();
                emit onUndo();
            }

            void redoAndNotify()
            {
                redo();
                emit onRedo();
            }

        private:

            // c is of type void(void)
            template<typename Callable>
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
                    emit undoTextChanged(m_undoable.top()->text());
                else
                    emit undoTextChanged("");

                if(canRedo())
                    emit redoTextChanged(m_redoable.top()->text());
                else
                    emit redoTextChanged("");

                emit indexChanged(m_undoable.size() - 1);
                emit stackChanged();
            }

            QStack<SerializableCommand*> m_undoable;
            QStack<SerializableCommand*> m_redoable;
    };
}
