#include <Process/LayerModel.hpp>
#include <Scenario/Document/Constraint/LayerModelLoader.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>


#include "RemoveLayerModelFromSlot.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

namespace Scenario
{
namespace Command
{

RemoveLayerModelFromSlot::RemoveLayerModelFromSlot(
        Path<SlotModel>&& rackPath,
        const Id<Process::LayerModel>& layerId) :
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

    auto lm = Process::createLayerModel(s,
                               slot.parentConstraint(),
                               &slot);
    slot.layers.add(lm);
}

void RemoveLayerModelFromSlot::redo() const
{
    auto& slot = m_path.find();
    slot.layers.remove(m_layerId);
}

void RemoveLayerModelFromSlot::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_layerId << m_serializedLayerData;
}

void RemoveLayerModelFromSlot::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_layerId >> m_serializedLayerData;
}

}
}
