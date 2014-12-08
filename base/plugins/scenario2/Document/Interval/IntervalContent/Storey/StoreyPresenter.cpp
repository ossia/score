#include "StoreyPresenter.hpp"
#include "StoreyModel.hpp"
#include "StoreyView.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <interface/process/ProcessPresenterInterface.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>
#include <Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <core/presenter/command/SerializableCommand.hpp>
#include <QGraphicsScene>
#include <tools/utilsCPP11.hpp>

// @todo vérifier en créant un nouvel élément
// qu'il n'existe pas déjà dans un tableau.
StoreyPresenter::StoreyPresenter(StoreyModel* model,
								 StoreyView* view,
								 QObject* parent):
	NamedObject{parent, "StoreyPresenter"},
	m_model{model},
	m_view{view}
{
	m_view->m_height = m_model->height();

	for(iscore::ProcessViewModelInterface* proc_vm : m_model->processViewModels())
	{
		on_processViewModelCreated_impl(proc_vm);
	}

	connect(this, SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));

	connect(this, SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));

	connect(m_model, SIGNAL(processViewModelCreated(int)),
			this,	SLOT(on_processViewModelCreated(int)));
	connect(m_model, SIGNAL(processViewModelDeleted(int)),
			this,	SLOT(on_processViewModelDeleted(int)));
}

StoreyPresenter::~StoreyPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

int StoreyPresenter::id() const
{
	return m_model->id();
}

void StoreyPresenter::on_processViewModelCreated(int processId)
{
	on_processViewModelCreated_impl(m_model->processViewModel(processId));
}

void StoreyPresenter::on_processViewModelDeleted(int processId)
{
	removeFromVectorWithId(m_processes, processId);
	m_view->update();
}

void StoreyPresenter::on_processViewModelCreated_impl(iscore::ProcessViewModelInterface* proc_vm)
{
	auto procname = m_model
						->parentInterval()
						->process(proc_vm->sharedProcessId())->processName();

	auto factory = iscore::ProcessList::getFactory(procname);

	auto proc_view = factory->makeView(factory->availableViews().first(), m_view);
	auto presenter = factory->makePresenter(proc_vm, proc_view, this);
	m_processes.push_back(presenter);
}
