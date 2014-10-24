#pragma once
#include <QUndoStack>
#include <core/application/SessionType.hpp>
#include <core/presenter/command/remote/RemoteActionEmitter.hpp>
#include <core/presenter/command/remote/RemoteActionReceiver.hpp>
#include <memory>

class Session;
namespace iscore
{
	class RemoteActionEmitter;
	class RemoteActionReceiver;
	class CommandQueue : public QUndoStack
	{
		public:
			CommandQueue(Session*);
			void push(Command *cmd);

		private:
			std::unique_ptr<RemoteActionEmitter> m_emitter;
			std::unique_ptr<RemoteActionReceiver> m_receiver;
	};
}
