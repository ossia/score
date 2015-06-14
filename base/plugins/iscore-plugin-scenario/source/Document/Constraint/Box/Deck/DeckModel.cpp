#include "DeckModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"

DeckModel::DeckModel(
        const id_type<DeckModel>& id,
        BoxModel* parent) :
    IdentifiedObject<DeckModel> {id, "DeckModel", parent}
{
}

DeckModel::DeckModel(
        std::function<void(const DeckModel&, DeckModel&)> pvmCopyMethod,
        const DeckModel& source,
        const id_type<DeckModel>& id,
        BoxModel *parent):
    IdentifiedObject<DeckModel> {id, "DeckModel", parent},
    m_frontProcessViewModelId {source.frontProcessViewModel() }, // Keep the same id.
    m_height {source.height() }
{
    pvmCopyMethod(source, *this);
}

void DeckModel::copyViewModelsInSameConstraint(
        const DeckModel &source,
        DeckModel &target)
{
    for(ProcessViewModel* pvm : source.processViewModels())
    {
        // We can safely reuse the same id since it's in a different deck.
        auto& proc = pvm->sharedProcessModel();
        target.addProcessViewModel(
                    proc.cloneViewModel(pvm->id(),
                                        *pvm,
                                        &target));
    }
}

void DeckModel::addProcessViewModel(ProcessViewModel* viewmodel)
{
    m_processViewModels.insert(viewmodel);

    emit processViewModelCreated(viewmodel->id());

    putToFront(viewmodel->id());
}

void DeckModel::deleteProcessViewModel(
        const id_type<ProcessViewModel>& processViewId)
{
    auto& pvm = processViewModel(processViewId);
    m_processViewModels.remove(processViewId);

    emit processViewModelRemoved(processViewId);


    if(!m_processViewModels.empty())
    {
        putToFront((*m_processViewModels.begin())->id());
    }
    else
    {
        m_frontProcessViewModelId.setVal({});
    }

    delete &pvm;
}

void DeckModel::putToFront(
        const id_type<ProcessViewModel>& processViewId)
{
    if(!processViewId.val())
        return;

    if(processViewId != m_frontProcessViewModelId)
    {
        m_frontProcessViewModelId = processViewId;
        emit processViewModelPutToFront(processViewId);
    }
}

const id_type<ProcessViewModel>& DeckModel::frontProcessViewModel() const
{
    return m_frontProcessViewModelId;
}

ProcessViewModel& DeckModel::processViewModel(
        const id_type<ProcessViewModel>& processViewModelId) const
{
    return *m_processViewModels.at(processViewModelId);
}

void DeckModel::on_deleteSharedProcessModel(
        const id_type<ProcessModel>& sharedProcessId)
{
    using namespace std;
    auto it = find_if(begin(m_processViewModels),
                      end(m_processViewModels),
                      [&sharedProcessId](const ProcessViewModel * pvm)
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

ConstraintModel& DeckModel::parentConstraint() const
{
    return static_cast<ConstraintModel&>(*parent()->parent());
}

int DeckModel::height() const
{
    return m_height;
}

bool DeckModel::focus() const
{
    return m_focus;
}
