#include <core/document/DocumentPresenter.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>



using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentModel* m, DocumentView* v, QObject* parent):
	NamedObject{"DocumentPresenter", parent},
	m_commandQueue{std::make_unique<CommandQueue>(this)},
	m_view{v},
	m_model{m}
{
}

void DocumentPresenter::newDocument()
{
}

void DocumentPresenter::applyCommand(SerializableCommand* cmd)
{
	m_commandQueue->pushAndEmit(cmd);
}

void DocumentPresenter::lock_impl()
{
	QByteArray arr;
	Serializer<DataStream> ser{&arr};
	ser.readFrom(m_lockedObject);
	emit lock(arr);
}

void DocumentPresenter::unlock_impl()
{
	QByteArray arr;
	Serializer<DataStream> ser{&arr};
	ser.readFrom(m_lockedObject);
	emit unlock(arr);
	m_lockedObject = ObjectPath();
}

void DocumentPresenter::initiateOngoingCommand(SerializableCommand* cmd, QObject* objectToLock)
{
	m_lockedObject = ObjectPath::pathFromObject(objectToLock);
	lock_impl();

	m_ongoingCommand = cmd;
	m_ongoingCommand->redo();
}

void DocumentPresenter::continueOngoingCommand(SerializableCommand* cmd)
{
	cmd->redo();
	m_ongoingCommand->mergeWith(cmd);
	delete cmd;
}

void DocumentPresenter::undoOngoingCommand()
{
	if(m_ongoingCommand)
	{
		unlock_impl();
		m_ongoingCommand->undo();
		delete m_ongoingCommand;
		m_ongoingCommand = nullptr;
	}
}

void DocumentPresenter::validateOngoingCommand()
{
	if(m_ongoingCommand)
	{
		unlock_impl();
		m_ongoingCommand->undo();
		m_commandQueue->pushAndEmit(m_ongoingCommand);
		m_ongoingCommand = nullptr;
	}
}

void DocumentPresenter::reset()
{
	m_commandQueue->clear();

	if(m_presenter)
	{
		m_presenter->deleteLater();
		m_presenter = nullptr;
	}

	m_view->reset();
}

void DocumentPresenter::setPresenterDelegate(DocumentDelegatePresenterInterface* pres)
{
	if(m_presenter) m_presenter->deleteLater();
	m_presenter = pres;

	connect(m_presenter, &DocumentDelegatePresenterInterface::submitCommand,
			this,		 &DocumentPresenter::applyCommand, Qt::QueuedConnection);

	connect(m_presenter, &DocumentDelegatePresenterInterface::elementSelected,
			this,		 &DocumentPresenter::on_elementSelected, Qt::QueuedConnection);
}

