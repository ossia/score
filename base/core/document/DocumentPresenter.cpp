#include <core/document/DocumentPresenter.hpp>

#include <API/Headers/Repartition/session/MasterSession.h>
#include <API/Headers/Repartition/session/ClientSessionBuilder.h>

using namespace iscore;


void DocumentPresenter::applyCommand(Command* cmd)
{
	m_commandQueue->push(cmd);
}

void DocumentPresenter::setupMasterSession()
{
	// TODO mettre un reset global ici


	m_networkSession = std::make_unique<MasterSession>("Session Maitre", 5678);
}

DocumentPresenter::DocumentPresenter(DocumentModel*, DocumentView*):
	m_networkSession{std::make_unique<MasterSession>("Session Maitre", 5678)}, // Va dans le DocumentPresenter
	m_commandQueue{std::make_unique<CommandQueue>(m_networkSession.get())}
{
}

void DocumentPresenter::setupClientSession(ConnectionData d)
{
	// TODO mettre un reset global ici (ou avant)

	m_networkSession.reset();
	qDebug(Q_FUNC_INFO);
	ClientSessionBuilder builder(d.remote_ip, d.remote_port, "JeanMi", 7888);
	builder.join();
	sleep(2);
	m_networkSession = builder.getBuiltSession();
}
