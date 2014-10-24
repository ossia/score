#include <core/presenter/command/CommandQueue.hpp>
#include <core/presenter/command/remote/RemoteActionEmitterClient.hpp>
#include <core/presenter/command/remote/RemoteActionEmitterMaster.hpp>
#include <core/presenter/command/remote/RemoteActionReceiver.hpp>

#include <API/Headers/Repartition/session/MasterSession.h>
#include <API/Headers/Repartition/session/ClientSession.h>

#include <core/presenter/command/Command.hpp>

using namespace iscore;

CommandQueue::CommandQueue(Session* s)
{
	if(dynamic_cast<ClientSession*>(s))
	{
		m_emitter = std::make_unique<RemoteActionEmitterClient>(static_cast<ClientSession*>(s));
	}
	else if(dynamic_cast<MasterSession*>(s))
	{
		m_emitter = std::make_unique<RemoteActionEmitterMaster>(static_cast<MasterSession*>(s));
	}
	else
	{
		qDebug("ALERT"), qDebug(Q_FUNC_INFO);
	}
}

void CommandQueue::push(Command* cmd)
{
	m_emitter->sendCommand(cmd);
	QUndoStack::push(cmd);
}
