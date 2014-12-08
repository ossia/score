#include <core/document/DocumentPresenter.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>



using namespace iscore;

DocumentPresenter::DocumentPresenter(QObject* parent, DocumentModel* m, DocumentView* v):
	NamedObject{parent, "DocumentPresenter"},
	m_commandQueue{std::make_unique<CommandQueue>(this)}
{
}

void DocumentPresenter::newDocument()
{
}

void DocumentPresenter::applyCommand(SerializableCommand* cmd)
{
	m_commandQueue->pushAndEmit(cmd);
}

void DocumentPresenter::reset()
{
	m_commandQueue->clear();
}

void DocumentPresenter::setPresenter(DocumentDelegatePresenterInterface* pres)
{
	if(m_presenter) m_presenter->deleteLater();
	m_presenter = pres;

	connect(m_presenter, &DocumentDelegatePresenterInterface::submitCommand,
			this,		 &DocumentPresenter::applyCommand, Qt::QueuedConnection);

	connect(m_presenter, &DocumentDelegatePresenterInterface::elementSelected,
			this,		 &DocumentPresenter::on_elementSelected, Qt::QueuedConnection);
}
