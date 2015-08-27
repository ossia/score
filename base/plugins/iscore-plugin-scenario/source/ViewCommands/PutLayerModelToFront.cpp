#include "PutLayerModelToFront.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

PutLayerModelToFront::PutLayerModelToFront(
        ModelPath<SlotModel>&& slotPath,
        const id_type<LayerModel>& pid):
    m_slotPath{std::move(slotPath)},
    m_pid{pid}
{

}

void PutLayerModelToFront::redo()
{
    m_slotPath.find().putToFront(m_pid);
}
