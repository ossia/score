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
	m_model{new DocumentModel{this}},
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
	// Model setup
	auto model = p->makeModel();
	m_model->setModel(model);

	// View setup
	auto view = p->makeView();
	auto lay = m_view->layout();
	auto widg = view->getWidget();
	lay->addWidget(widg);

	// Presenter setup
	auto pres = p->makePresenter(m_presenter, model, view);
	m_presenter->setPresenter(pres);
}

void Document::reset()
{
	m_presenter->reset();
	m_model->reset();
}
