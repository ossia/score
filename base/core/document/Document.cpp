#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

using namespace iscore;
Document::Document(QObject* parent):
	QObject{parent},
	m_model{new DocumentModel},
	m_view{new DocumentView},
	m_presenter{new DocumentPresenter(m_model, m_view)}
{

}
