#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "Control/ProcessList.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/utilsCPP11.hpp>

#include <QDebug>

QDataStream& operator << (QDataStream& s, const DeckModel& deck)
{
	s << static_cast<const IdentifiedObject&>(deck);

	s << deck.m_editedProcessId;

	s << (int) deck.m_processViewModels.size();
	for(auto& pvm : deck.m_processViewModels)
	{
		s << pvm->sharedProcessModel()->id(); // TODO put this in the qdatastream ctor of pvm interface
		s << *pvm;
	}

	s << deck.height()
	  << deck.position();
	return s;
}


QDataStream& operator >> (QDataStream& s, DeckModel& deck)
{
	int editedProcessId;
	s >> editedProcessId;

	int pvm_size;
	s >> pvm_size;
	for(int i = 0; i < pvm_size; i++)
	{
		SettableIdentifier sharedprocess_id;
		s >> sharedprocess_id;
		deck.createProcessViewModel(s, sharedprocess_id);
	}

	int height;
	int position;
	s >> height
	  >> position;
	deck.setHeight(height);
	deck.setPosition(position);

	deck.selectForEdition(editedProcessId);

	return s;
}

DeckModel::DeckModel(QDataStream& s, BoxModel* parent):
	IdentifiedObject{s, "DeckModel", parent}
{
	s >> *this;
}

DeckModel::DeckModel(int position, int id, BoxModel* parent):
	IdentifiedObject{id, "DeckModel", parent},
	m_position{position}
{
}

// TODO refactor this like in the presenter classes with _impl.
int DeckModel::createProcessViewModel(int sharedProcessId, int newProcessViewModelId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(newProcessViewModelId, this);

	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated(viewmodel->id());
	return viewmodel->id();
}

int DeckModel::createProcessViewModel(QDataStream& s, int sharedProcessId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(s, this);

	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated(viewmodel->id());
	return viewmodel->id();
}

void DeckModel::deleteProcessViewModel(int processViewId)
{
	removeById(m_processViewModels, processViewId);
	emit processViewModelRemoved(processViewId);
}

void DeckModel::selectForEdition(int processViewId)
{
	if(processViewId != m_editedProcessId)
	{
		m_editedProcessId = processViewId;
		emit processViewModelSelected(processViewId);
	}
}

const std::vector<ProcessViewModelInterface*>&DeckModel::processViewModels() const
{
	return m_processViewModels;
}

ProcessViewModelInterface*DeckModel::processViewModel(int processViewModelId) const
{
	return findById(m_processViewModels, processViewModelId);
}

void DeckModel::on_deleteSharedProcessModel(int sharedProcessId)
{
	// We HAVE to do a copy here because deleteProcessViewModel use the erase-remove idiom.
	auto viewmodels = m_processViewModels;

	for(auto process_vm : viewmodels)
	{
		if(process_vm->sharedProcessModel()->id() == sharedProcessId)
		{
			deleteProcessViewModel(process_vm->id());
		}
	}
}

void DeckModel::setHeight(int arg)
{
	if (m_height != arg) {
		m_height = arg;
		emit heightChanged(arg);
	}
}

void DeckModel::setPosition(int arg)
{
	if (m_position == arg)
		return;

	m_position = arg;
	emit positionChanged(arg);
}

ConstraintModel* DeckModel::parentConstraint() const
{
	return static_cast<ConstraintModel*>(parent()->parent());
	// TODO Is there a better way to do this ? Without breaking encapsulation ?
	// And without generating another ton of code from constraintmodel to deckmodel ?
}

int DeckModel::height() const
{
	return m_height;
}

int DeckModel::position() const
{
	return m_position;
}




ConstraintModel* parentConstraint(ProcessViewModelInterface* pvm)
{
	auto deck = dynamic_cast<DeckModel*>(pvm->parent());
	if(deck)
	{
		return deck->parentConstraint();
	}

	return nullptr;
}
