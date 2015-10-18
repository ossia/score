#include "RemoveLayerModelFromSlot.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/Process.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "source/ProcessInterfaceSerialization/LayerModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveLayerModelFromSlot::RemoveLayerModelFromSlot(
        Path<SlotModel>&& rackPath,
        const Id<LayerModel>& layerId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {rackPath},
    m_layerId {layerId}
{
    auto& slot = m_path.find();

    Serializer<DataStream> s{&m_serializedLayerData};
    s.readFrom(slot.layers.at(m_layerId));
}

void RemoveLayerModelFromSlot::undo() const
{
    auto& slot = m_path.find();
    Deserializer<DataStream> s {m_serializedLayerData};

    auto lm = createLayerModel(s,
                               slot.parentConstraint(),
                               &slot);
    slot.layers.add(lm);
}

void RemoveLayerModelFromSlot::redo() const
{
    auto& slot = m_path.find();
    slot.layers.remove(m_layerId);
}

void RemoveLayerModelFromSlot::serializeImpl(QDataStream& s) const
{
    s << m_path << m_layerId << m_serializedLayerData;
}

void RemoveLayerModelFromSlot::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_layerId >> m_serializedLayerData;
}
