#pragma once
#include <QUndoStack>
#include <core/presenter/command/remote/RemoteActionEmitter.hpp>
#include <core/presenter/command/remote/RemoteActionReceiver.hpp>
namespace iscore
{
	class CommandQueue : public QUndoStack
	{

		private:
			RemoteActionEmitter m_emitter;
			RemoteActionReceiver m_receiver;
	};
}
