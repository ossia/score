#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <iscore/tools/utilsCPP11.hpp>

#include <QDebug>

DeckModel::DeckModel(id_type<DeckModel> id, BoxModel* parent) :
    IdentifiedObject<DeckModel> {id, "DeckModel", parent}
{
}

DeckModel::DeckModel(DeckModel* source, id_type<DeckModel> id, BoxModel* parent) :
    IdentifiedObject<DeckModel> {id, "DeckModel", parent},
                 m_editedProcessViewModelId {source->editedProcessViewModel() }, // Keep the same id.
m_height {source->height() }
{
    for(ProcessViewModelInterface* pvm : source->processViewModels())
    {
        // We can safely reuse the same id since it's in a different deck.
        auto proc = pvm->sharedProcessModel();
        addProcessViewModel(
            proc->makeViewModel(pvm->id(),
        pvm,
        this));
    }
}

void DeckModel::addProcessViewModel(ProcessViewModelInterface* viewmodel)
{
    m_processViewModels.push_back(viewmodel);

    emit processViewModelCreated(viewmodel->id());
}

void DeckModel::deleteProcessViewModel(id_type<ProcessViewModelInterface> processViewId)
{
    auto pvm = processViewModel(processViewId);

    emit processViewModelRemoved(processViewId);

    vec_erase_remove_if(m_processViewModels,
                        [&processViewId](ProcessViewModelInterface * model)
    {
        return model->id() == processViewId;
    });


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
    if(!processViewId.val())
    {
        return;
    }

    if(processViewId != m_editedProcessViewModelId)
    {
        m_editedProcessViewModelId = processViewId;
        emit processViewModelSelected(processViewId);
    }
}

const std::vector<ProcessViewModelInterface*>& DeckModel::processViewModels() const
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
                      [&sharedProcessId](const ProcessViewModelInterface * pvm)
    {
        return pvm->sharedProcessModel()->id() == sharedProcessId;
    });

    if(it != end(m_processViewModels))
    {
        deleteProcessViewModel((*it)->id());
    }
}

void DeckModel::setHeight(int arg)
{
    if(m_height != arg)
    {
        m_height = arg;
        emit heightChanged(arg);
    }
}

ConstraintModel* DeckModel::parentConstraint() const
{
    return static_cast<ConstraintModel*>(parent()->parent());
}

int DeckModel::height() const
{
    return m_height;
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
