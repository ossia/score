#include "StoreyModel.hpp"
#include "Interval/IntervalContent/IntervalContentModel.hpp"
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <interface/process/ProcessViewModelInterface.hpp>
#include <Interval/IntervalModel.hpp>

#include <utilsCPP11.hpp>
#include <QDebug>
QDataStream& operator << (QDataStream& s, const StoreyModel& storey)
{
	qDebug() << Q_FUNC_INFO;
	s << storey.id() 
	  << storey.m_editedProcessId;
	
	s << (int) storey.m_processViewModels.size();
	for(auto& pvm : storey.m_processViewModels)
	{
		s << *pvm;
	}
	s << storey.m_nextProcessViewModelId;
}

StoreyModel::StoreyModel(int id, IntervalContentModel* parent):
	QIdentifiedObject{parent, "StoreyModel", id}
{
	
}

int StoreyModel::createProcessViewModel(int sharedProcessId)
{
	// Search the corresponding process in the parent interval.
	auto process = parentInterval()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(m_nextProcessViewModelId++, sharedProcessId, this);
	
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

void StoreyModel::on_deleteSharedProcessModel(int sharedProcessId)
{
	// We HAVE to do a copy here because deleteProcessViewModel use the erase-remove idiom.
	auto viewmodels = m_processViewModels; 
	
	for(auto process_vm : viewmodels)
	{
		if(process_vm->sharedProcessId() == sharedProcessId)
		{
			deleteProcessViewModel(process_vm->id());
		}
	}
}

#include "Interval/IntervalModel.hpp"
IntervalModel* StoreyModel::parentInterval()
{
	return static_cast<IntervalModel*>(parent()->parent()); // hello granpa
	// Is there a better way to do this ? Without breaking encapsulation ?
	// And without generating another ton of code from intervalmodel to storeymodel ?
}
