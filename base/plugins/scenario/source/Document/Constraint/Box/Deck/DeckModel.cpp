#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/utilsCPP11.hpp>

#include <QDebug>

DeckModel::DeckModel(int position, int id, BoxModel* parent):
	IdentifiedObject{id, "DeckModel", parent},
	m_position{position}
{
}

void DeckModel::createProcessViewModel(int sharedProcessId, int newProcessViewModelId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(newProcessViewModelId, this);

	addProcessViewModel(viewmodel);
}

void DeckModel::addProcessViewModel(ProcessViewModelInterface* viewmodel)
{
	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated((SettableIdentifier::identifier_type) viewmodel->id());
}

void DeckModel::deleteProcessViewModel(int processViewId)
{
	auto pvm = processViewModel(processViewId);
	vec_erase_remove_if(m_processViewModels,
						[&processViewId] (ProcessViewModelInterface* model)
						{ return model->id() == processViewId; });

	emit processViewModelRemoved(processViewId);

	m_editedProcessViewModelId = 0; // Todo use boost::optional

	delete pvm;
}

void DeckModel::selectForEdition(int processViewId)
{
	if(processViewId != m_editedProcessViewModelId)
	{
		m_editedProcessViewModelId = processViewId;
		emit processViewModelSelected(processViewId);
	}
}

const std::vector<ProcessViewModelInterface*>&DeckModel::processViewModels() const
{
	return m_processViewModels;
}

ProcessViewModelInterface* DeckModel::processViewModel(int processViewModelId) const
{
	return findById(m_processViewModels, processViewModelId);
}

void DeckModel::on_deleteSharedProcessModel(int sharedProcessId)
{
	using namespace std;
	// We HAVE to do a copy here because deleteProcessViewModel use the erase-remove idiom.
	auto viewmodels = m_processViewModels;
	auto it = find_if(begin(m_processViewModels),
					  end(m_processViewModels),
					  [&sharedProcessId] (const ProcessViewModelInterface* pvm)
						{ return pvm->sharedProcessModel()->id() == sharedProcessId; });

	if(it != end(m_processViewModels))
	{
		deleteProcessViewModel((SettableIdentifier::identifier_type) (*it)->id());
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
