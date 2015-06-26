#include "RemoveProcessViewModelFromSlot.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessViewModelFromSlot::RemoveProcessViewModelFromSlot(
        ObjectPath&& boxPath,
        id_type<ProcessViewModel> processViewId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {boxPath},
    m_processViewId {processViewId}
{
    auto& slot = m_path.find<SlotModel>();

    Serializer<DataStream> s{&m_serializedProcessViewData};
    s.readFrom(slot.processViewModel(m_processViewId));
}

void RemoveProcessViewModelFromSlot::undo()
{
    auto& slot = m_path.find<SlotModel>();
    Deserializer<DataStream> s {m_serializedProcessViewData};

    auto pvm = createProcessViewModel(s,
                                      slot.parentConstraint(),
                                      &slot);
    slot.addProcessViewModel(pvm);
}

void RemoveProcessViewModelFromSlot::redo()
{
    auto& slot = m_path.find<SlotModel>();
    slot.deleteProcessViewModel(m_processViewId);
}

void RemoveProcessViewModelFromSlot::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processViewId << m_serializedProcessViewData;
}

void RemoveProcessViewModelFromSlot::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processViewId >> m_serializedProcessViewData;
}
