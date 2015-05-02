#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

DeckModel::DeckModel(id_type<DeckModel> id, BoxModel* parent) :
    IdentifiedObject<DeckModel> {id, "DeckModel", parent}
{
}

DeckModel::DeckModel(
        std::function<void(DeckModel&, DeckModel&)> pvmCopyMethod,
        DeckModel *source,
        id_type<DeckModel> id,
        BoxModel *parent):
    IdentifiedObject<DeckModel> {id, "DeckModel", parent},
    m_editedProcessViewModelId {source->editedProcessViewModel() }, // Keep the same id.
    m_height {source->height() }
{
    pvmCopyMethod(*source, *this);
}

void DeckModel::copyViewModelsInSameConstraint(DeckModel &source, DeckModel &target)
{
    for(ProcessViewModelInterface* pvm : source.processViewModels())
    {
        // We can safely reuse the same id since it's in a different deck.
        auto& proc = pvm->sharedProcessModel();
        target.addProcessViewModel(
                    proc.cloneViewModel(pvm->id(),
                                        *pvm,
                                        &target));
    }
}

void DeckModel::addProcessViewModel(ProcessViewModelInterface* viewmodel)
{
    m_processViewModels.push_back(viewmodel);

    emit processViewModelCreated(viewmodel->id());

    selectForEdition(viewmodel->id());
}

void DeckModel::deleteProcessViewModel(id_type<ProcessViewModelInterface> processViewId)
{
    auto pvm = processViewModel(processViewId);

    vec_erase_remove_if(m_processViewModels,
                        [&processViewId](ProcessViewModelInterface * model)
    {
        return model->id() == processViewId;
    });

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
    Q_ASSERT(processViewId.val());

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
        return pvm->sharedProcessModel().id() == sharedProcessId;
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

void DeckModel::setFocus(bool arg)
{
    if (m_focus == arg)
        return;

    m_focus = arg;
    emit focusChanged(arg);
}

ConstraintModel* DeckModel::parentConstraint() const
{
    return static_cast<ConstraintModel*>(parent()->parent());
}

int DeckModel::height() const
{
    return m_height;
}

bool DeckModel::focus() const
{
    return m_focus;
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
