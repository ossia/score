#pragma once
#include <QUndoStack>
#include <core/presenter/command/SerializableCommand.hpp>
#include <memory>

class Session;
namespace iscore
{
	/**
	 * @brief The CommandQueue class
	 *
	 * Mostly equivalent to QUndoStack, but has added signals / slots.
	 * They are used to send & receive the commands to the network, for instance.
	 */
	class CommandQueue : public QUndoStack
	{
			Q_OBJECT
		public:
			CommandQueue();

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

		public slots:
			/**
			 * @brief push Pushes a command on the stack
			 * @param cmd The command
			 *
			 * Calls QUndoStack::push
			 */
			void push(iscore::SerializableCommand* cmd);

			/**
			 * @brief pushAndEmit Pushes a command on the stack and emit relevant signals
			 * @param cmd The command
			 */
			void pushAndEmit(iscore::SerializableCommand* cmd);
	};
}
