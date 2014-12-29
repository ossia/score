#include <core/document/DocumentPresenter.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>



using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentModel* m, DocumentView* v, QObject* parent):
	NamedObject{"DocumentPresenter", parent},
	m_commandQueue{std::make_unique<CommandQueue>(this)},
	m_view{v}
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
