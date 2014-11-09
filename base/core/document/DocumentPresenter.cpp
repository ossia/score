#include <core/document/DocumentPresenter.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/utilsCPP11.hpp>


using namespace iscore;

DocumentPresenter::DocumentPresenter(QObject* parent, DocumentModel* m, DocumentView* v):
	QObject{parent},
	m_commandQueue{std::make_unique<CommandQueue>()}
{
	m_commandQueue->setParent(this);
}

void DocumentPresenter::newDocument()
{
}

void DocumentPresenter::applyCommand(Command* cmd)
{
	m_commandQueue->push(cmd);
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
			this, &DocumentPresenter::applyCommand);
}
