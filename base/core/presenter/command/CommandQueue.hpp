#pragma once
#include <QUndoStack>
#include <core/presenter/command/Command.hpp>
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
			void push_start(iscore::Command* cmd);

			void onUndo();
			void onRedo();

		public slots:
			void push(iscore::Command* cmd);
			void pushAndEmit(iscore::Command* cmd);


	};
}
