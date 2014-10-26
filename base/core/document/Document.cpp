#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

using namespace iscore;
Document::Document(QObject* parent, QWidget* parentview):
	QObject{parent},
	m_model{new DocumentModel},
	m_view{new DocumentView{parentview}},
	m_presenter{new DocumentPresenter(this, m_model, m_view)}
{
	setObjectName("Document");
}

void Document::newDocument()
{
	reset();

	emit newDocument_start();
}

void Document::reset()
{
	m_presenter->reset();
	m_model->reset();
}
