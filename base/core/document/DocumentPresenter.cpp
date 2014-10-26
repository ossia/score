#include <core/document/DocumentPresenter.hpp>

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
