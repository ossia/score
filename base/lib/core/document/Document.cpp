#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

#include <QDebug>
#include <QLayout>

using namespace iscore;
Document::Document(QWidget* parentview, QObject* parent):
	NamedObject{"Document", parent},
	m_model{new DocumentModel{this}},
	m_view{new DocumentView{parentview}},
	m_presenter{new DocumentPresenter(m_model, m_view, this)}
{
	connect(m_presenter, &DocumentPresenter::on_elementSelected,
			this,		 &Document::on_elementSelected);
}

void Document::newDocument()
{
	reset();

	emit newDocument_start();
}

void Document::setDocumentPanel(DocumentDelegateFactoryInterface* p)
{
	// Model setup
	auto model = p->makeModel(m_model);
	m_model->setModelDelegate(model);

	// View setup
	auto view = p->makeView(m_view);
	m_view->setViewDelegate(view);

	// Presenter setup
	auto pres = p->makePresenter(m_presenter, model, view);
	m_presenter->setPresenterDelegate(pres);
}

void Document::reset()
{
	m_presenter->reset();
	m_model->reset();
}
