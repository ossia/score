#include "StoreyModel.hpp"
#include "Interval/IntervalContent/IntervalContentModel.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <Interval/IntervalModel.hpp>

#include <utilsCPP11.hpp>

StoreyModel::StoreyModel(int id, IntervalContentModel* parent):
	QNamedObject{parent, "StoreyModel"},
	m_id{id}
{
	
}

void StoreyModel::createProcessViewModel(int processId)
{
	// Search the corresponding process in the parent interval.
	auto process = parentInterval()->process(processId);
	auto viewmodel = process->makeViewModel(m_nextProcessViewModelId, this);
	
	m_processViewModels.push_back(viewmodel);
	
	emit processViewModelCreated(viewmodel->id());
}

void StoreyModel::deleteProcessViewModel(int processViewId)
{
	emit processViewModelDeleted(processViewId);
	
	vec_erase_remove_if(m_processViewModels, 
					   [&processViewId] (iscore::ProcessViewModelInterface* model) 
						  { 
							  bool to_delete = model->id() == processViewId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
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
