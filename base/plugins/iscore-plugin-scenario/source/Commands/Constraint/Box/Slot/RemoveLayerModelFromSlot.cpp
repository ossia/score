#include "RemoveLayerModelFromSlot.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "source/ProcessInterfaceSerialization/LayerModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveLayerModelFromSlot::RemoveLayerModelFromSlot(
        ObjectPath&& boxPath,
        id_type<LayerModel> layerId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {boxPath},
    m_layerId {layerId}
{
    auto& slot = m_path.find<SlotModel>();

    Serializer<DataStream> s{&m_serializedLayerData};
    s.readFrom(slot.layerModel(m_layerId));
}

void RemoveLayerModelFromSlot::undo()
{
    auto& slot = m_path.find<SlotModel>();
    Deserializer<DataStream> s {m_serializedLayerData};

    auto pvm = createLayerModel(s,
                                      slot.parentConstraint(),
                                      &slot);
    slot.addLayerModel(pvm);
}

void RemoveLayerModelFromSlot::redo()
{
    auto& slot = m_path.find<SlotModel>();
    slot.deleteLayerModel(m_layerId);
}

void RemoveLayerModelFromSlot::serializeImpl(QDataStream& s) const
{
    s << m_path << m_layerId << m_serializedLayerData;
}

void RemoveLayerModelFromSlot::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_layerId >> m_serializedLayerData;
}
