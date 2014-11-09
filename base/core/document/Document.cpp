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

void Document::setDocumentPanel(DocumentDelegateFactoryInterface* p)
{
	auto model = p->makeModel();
	auto view = p->makeView();
	auto pres = p->makePresenter(m_presenter, model, view);

	view->setPresenter(pres);
	model->setPresenter(pres);

	// View setup
	auto lay = m_view->layout();
	auto widg = view->getWidget();
	lay->addWidget(widg);

	// Model setup
	m_model->setModel(model);

	// Presenter setup
	m_presenter->setPresenter(pres);
}

void Document::reset()
{
	m_presenter->reset();
	m_model->reset();
}
