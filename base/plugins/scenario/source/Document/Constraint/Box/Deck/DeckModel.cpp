#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/utilsCPP11.hpp>

#include <QDebug>

DeckModel::DeckModel(int position, id_type<DeckModel> id, BoxModel* parent):
	IdentifiedObject<DeckModel>{id, "DeckModel", parent},
	m_position{position}
{
}

void DeckModel::createProcessViewModel(id_type<ProcessSharedModelInterface> sharedProcessId, id_type<ProcessViewModelInterface> newProcessViewModelId)
{
	// Search the corresponding process in the parent constraint.
	auto process = parentConstraint()->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(newProcessViewModelId, this);

	addProcessViewModel(viewmodel);
}

void DeckModel::addProcessViewModel(ProcessViewModelInterface* viewmodel)
{
	m_processViewModels.push_back(viewmodel);

	emit processViewModelCreated(viewmodel->id());
}

void DeckModel::deleteProcessViewModel(id_type<ProcessViewModelInterface> processViewId)
{
	auto pvm = processViewModel(processViewId);
	vec_erase_remove_if(m_processViewModels,
						[&processViewId] (ProcessViewModelInterface* model)
						{ return model->id() == processViewId; });

	emit processViewModelRemoved(processViewId);

	if(!m_processViewModels.empty())
	{
		selectForEdition((*m_processViewModels.begin())->id());
	}
	else
	{
		m_editedProcessViewModelId.setVal({});
	}

	delete pvm;
}

void DeckModel::selectForEdition(id_type<ProcessViewModelInterface> processViewId)
{
	if(!processViewId.val().is_initialized())
	{
		// TODO no pvm
		return;
	}
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

ProcessViewModelInterface* DeckModel::processViewModel(id_type<ProcessViewModelInterface> processViewModelId) const
{
	return findById(m_processViewModels, processViewModelId);
}

void DeckModel::on_deleteSharedProcessModel(id_type<ProcessSharedModelInterface> sharedProcessId)
{
	using namespace std;
	auto it = find_if(begin(m_processViewModels),
					  end(m_processViewModels),
					  [&sharedProcessId] (const ProcessViewModelInterface* pvm)
						{ return pvm->sharedProcessModel()->id() == sharedProcessId; });

	if(it != end(m_processViewModels))
	{
		deleteProcessViewModel((*it)->id());
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
