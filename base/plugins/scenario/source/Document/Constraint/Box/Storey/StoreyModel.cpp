#include "StoreyModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "Control/ProcessList.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/utilsCPP11.hpp>

#include <QDebug>

QDataStream& operator << (QDataStream& s, const StoreyModel& storey)
{
	s << storey.m_editedProcessId;

	s << (int) storey.m_processViewModels.size();
	for(auto& pvm : storey.m_processViewModels)
	{
		s << pvm->sharedProcessId();
		s << *pvm;
	}

	s << storey.height()
	  << storey.position();

	return s;
}


QDataStream& operator >> (QDataStream& s, StoreyModel& storey)
{
	int editedProcessId;
	s >> editedProcessId;

	int pvm_size;
	s >> pvm_size;
	for(int i = 0; i < pvm_size; i++)
	{
		int sharedprocess_id;
		s >> sharedprocess_id;
		storey.createProcessViewModel(s, sharedprocess_id);
	}

	int height;
	int position;
	s >> height
	  >> position;
	storey.setHeight(height);
	storey.setPosition(position);

	storey.selectForEdition(editedProcessId);

	return s;
}

StoreyModel::StoreyModel(QDataStream& s, BoxModel* parent):
	IdentifiedObject{s, "StoreyModel", parent}
{
	s >> *this;
}

StoreyModel::StoreyModel(int position, int id, BoxModel* parent):
	IdentifiedObject{id, "StoreyModel", parent},
	m_position{position}
{

}

int StoreyModel::createProcessViewModel(int sharedProcessId, int newProcessViewModelId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(newProcessViewModelId, sharedProcessId, this);

	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated(viewmodel->id());
	return viewmodel->id();
}

int StoreyModel::createProcessViewModel(QDataStream& s, int sharedProcessId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(s, this);

	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated(viewmodel->id());
	return viewmodel->id();
}

void StoreyModel::deleteProcessViewModel(int processViewId)
{
	emit processViewModelDeleted(processViewId);

	removeById(m_processViewModels, processViewId);
}

void StoreyModel::selectForEdition(int processViewId)
{
	if(processViewId != m_editedProcessId)
	{
		m_editedProcessId = processViewId;
		emit processViewModelSelected(processViewId);
	}
}

const std::vector<ProcessViewModelInterface*>&StoreyModel::processViewModels() const
{
	return m_processViewModels;
}

ProcessViewModelInterface*StoreyModel::processViewModel(int processViewModelId) const
{
	return findById(m_processViewModels, processViewModelId);
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

void StoreyModel::setHeight(int arg)
{
	if (m_height != arg) {
		m_height = arg;
		emit heightChanged(arg);
	}
}

void StoreyModel::setPosition(int arg)
{
	if (m_position == arg)
		return;

	m_position = arg;
	emit positionChanged(arg);
}

ConstraintModel* StoreyModel::parentConstraint() const
{
	return static_cast<ConstraintModel*>(parent()->parent());
	// TODO Is there a better way to do this ? Without breaking encapsulation ?
	// And without generating another ton of code from constraintmodel to storeymodel ?
}

int StoreyModel::height() const
{
	return m_height;
}

int StoreyModel::position() const
{
	return m_position;
}
