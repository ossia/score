#include "StoreyModel.hpp"
#include "Interval/IntervalContent/IntervalContentModel.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <Interval/IntervalModel.hpp>

#include <utilsCPP11.hpp>

StoreyModel::StoreyModel(int id, IntervalContentModel* parent):
	QIdentifiedObject{parent, "StoreyModel", id}
{
	
}

int StoreyModel::createProcessViewModel(int processId)
{
	// Search the corresponding process in the parent interval.
	auto process = parentInterval()->process(processId);
	auto viewmodel = process->makeViewModel(m_nextProcessViewModelId++, this);
	
	m_processViewModels.push_back(viewmodel);
	
	emit processViewModelCreated(viewmodel->id());
	return viewmodel->id();
}

void StoreyModel::deleteProcessViewModel(int processViewId)
{
	emit processViewModelDeleted(processViewId);
	
	removeById(m_processViewModels, processViewId);
	m_nextProcessViewModelId--;
}

void StoreyModel::selectForEdition(int processViewId)
{
	if(processViewId != m_editedProcessId)
	{
		m_editedProcessId = processViewId;
		emit processViewModelSelected(processViewId);
	}
}

#include "Interval/IntervalModel.hpp"
IntervalModel* StoreyModel::parentInterval()
{
	return static_cast<IntervalModel*>(parent()->parent()); // hello granpa
	// Is there a better way to do this ? Without breaking encapsulation ?
	// And without generating another ton of code from intervalmodel to storeymodel ?
}
