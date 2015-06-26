#include "PutProcessViewModelToFront.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

PutProcessViewModelToFront::PutProcessViewModelToFront(
        ObjectPath&& slotPath,
        const id_type<ProcessViewModel>& pid):
    m_slotPath{std::move(slotPath)},
    m_pid{pid}
{

}

void PutProcessViewModelToFront::redo()
{
    m_slotPath.find<SlotModel>().putToFront(m_pid);
}
