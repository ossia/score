#include <core/document/DocumentPresenter.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/utilsCPP11.hpp>

void parentHierarchy(QObject* obj)
{
	while(obj)
	{
		qDebug() << obj->objectName();
		obj = obj->parent();
	}
}

using namespace iscore;

DocumentPresenter::DocumentPresenter(QObject* parent, DocumentModel* m, DocumentView* v):
	QNamedObject{parent, "DocumentPresenter"},
	m_commandQueue{std::make_unique<CommandQueue>()}
{
	parentHierarchy(this);
	m_commandQueue->setParent(this);
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
			this, &DocumentPresenter::applyCommand, Qt::QueuedConnection);
}
