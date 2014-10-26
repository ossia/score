#pragma once
#include <QUndoStack>
#include <core/presenter/command/Command.hpp>
#include <memory>

class Session;
namespace iscore
{
	class CommandQueue : public QUndoStack
	{
			Q_OBJECT
		public:
			CommandQueue();
			void push(Command *cmd);

		signals:
			void push_start(iscore::Command* cmd);

		public slots:
			void receiveCommand(iscore::Command* cmd);
	};
}
