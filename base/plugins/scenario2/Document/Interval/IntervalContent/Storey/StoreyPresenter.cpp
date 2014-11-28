#include "StoreyPresenter.hpp"
#include "StoreyModel.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>
#include <Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>

StoreyPresenter::StoreyPresenter(StoreyModel* const model, QObject* parent):
	QNamedObject{parent, "StoreyPresenter"},
	m_model{model}
{
	for(iscore::ProcessViewModelInterface* pvm : m_model->processViewModels())
	{
		auto procname = m_model->parentInterval()->process(pvm->sharedProcessId())->processName();
		auto factory = iscore::ProcessList::getFactory(procname);

		auto presenter = factory->makePresenter(pvm, this);
		m_processes.push_back(presenter);



	}
}
