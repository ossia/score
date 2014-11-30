#include "StoreyPresenter.hpp"
#include "StoreyModel.hpp"
#include "StoreyView.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>
#include <Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <core/presenter/command/SerializableCommand.hpp>
#include <QGraphicsScene>
StoreyPresenter::StoreyPresenter(StoreyModel* model,
								 StoreyView* view,
								 QObject* parent):
	QNamedObject{parent, "StoreyPresenter"},
	m_model{model},
	m_view{view}
{
	m_view->m_height = m_model->height();

	for(iscore::ProcessViewModelInterface* proc_vm : m_model->processViewModels())
	{
		auto procname = m_model->parentInterval()->process(proc_vm->sharedProcessId())->processName();
		auto factory = iscore::ProcessList::getFactory(procname);

		auto proc_view = factory->makeView(factory->availableViews().first(), view);
		auto presenter = factory->makePresenter(proc_vm, proc_view, this);
		m_processes.push_back(presenter);
	}

	connect(this, SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));
}

StoreyPresenter::~StoreyPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}
