#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>

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
	m_presenter{new DocumentPresenter{m_model, m_view, this}}
{
	connect(m_presenter, &DocumentPresenter::on_elementSelected,
			this,		 &Document::on_elementSelected);
}

void Document::newDocument()
{
	reset();

	// Model setup
	m_model->setModelDelegate(m_currentDocumentType->makeModel(m_model));
	setupDocument();

	emit newDocument_start();
}

void Document::setDocumentPanel(DocumentDelegateFactoryInterface* p)
{
	m_currentDocumentType = p;
}

void Document::setupPanel(PanelPresenterInterface* pres, PanelFactoryInterface* factory)
{
	auto model = factory->makeModel(m_model);
	m_model->addPanel(model);

	pres->setModel(model);
}


void Document::reset()
{
	m_model->reset();
	m_presenter->reset();
}

void Document::load(QByteArray data)
{
	reset();

	// Model setup
	m_model->setModelDelegate(m_currentDocumentType->makeModel(m_model, data));

	// TODO call newDocument_start if loaded from this computer, not if serialized from network.
	setupDocument();
}

QByteArray Document::save()
{
	return m_model->modelDelegate()->save();
}

void Document::setupDocument()
{
	// View setup
	auto view = m_currentDocumentType->makeView(m_view);
	m_view->setViewDelegate(view);

	// Presenter setup
	auto pres = m_currentDocumentType->makePresenter(m_presenter, m_model->modelDelegate(), view);
	m_presenter->setPresenterDelegate(pres);
}
