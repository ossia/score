#include "SlotModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"

SlotModel::SlotModel(
        const id_type<SlotModel>& id,
        BoxModel* parent) :
    IdentifiedObject<SlotModel> {id, "SlotModel", parent}
{
}

SlotModel::SlotModel(
        std::function<void(const SlotModel&, SlotModel&)> pvmCopyMethod,
        const SlotModel& source,
        const id_type<SlotModel>& id,
        BoxModel *parent):
    IdentifiedObject<SlotModel> {id, "SlotModel", parent},
    m_frontProcessViewModelId {source.frontProcessViewModel() }, // Keep the same id.
    m_height {source.height() }
{
    pvmCopyMethod(source, *this);
}

void SlotModel::copyViewModelsInSameConstraint(
        const SlotModel &source,
        SlotModel &target)
{
    for(ProcessViewModel* pvm : source.processViewModels())
    {
        // We can safely reuse the same id since it's in a different slot.
        auto& proc = pvm->sharedProcessModel();
        target.addProcessViewModel(
                    proc.cloneViewModel(pvm->id(),
                                        *pvm,
                                        &target));
    }
}

void SlotModel::addProcessViewModel(ProcessViewModel* viewmodel)
{
    m_processViewModels.insert(viewmodel);

    emit processViewModelCreated(viewmodel->id());

    putToFront(viewmodel->id());
}

void SlotModel::deleteProcessViewModel(
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

void SlotModel::putToFront(
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

const id_type<ProcessViewModel>& SlotModel::frontProcessViewModel() const
{
    return m_frontProcessViewModelId;
}

ProcessViewModel& SlotModel::processViewModel(
        const id_type<ProcessViewModel>& processViewModelId) const
{
    return *m_processViewModels.at(processViewModelId);
}

void SlotModel::on_deleteSharedProcessModel(
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

void SlotModel::setHeight(int arg)
{
    if(m_height != arg)
    {
        m_height = arg;
        emit heightChanged(arg);
    }
}

void SlotModel::setFocus(bool arg)
{
    if (m_focus == arg)
        return;

    m_focus = arg;
    emit focusChanged(arg);
}

ConstraintModel& SlotModel::parentConstraint() const
{
    return static_cast<ConstraintModel&>(*parent()->parent());
}

int SlotModel::height() const
{
    return m_height;
}

bool SlotModel::focus() const
{
    return m_focus;
}
