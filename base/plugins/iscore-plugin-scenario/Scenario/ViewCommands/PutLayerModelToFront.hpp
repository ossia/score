#pragma once
#include <iscore/tools/ModelPath.hpp>

class LayerModel;
class SlotModel;
#include <iscore/tools/SettableIdentifier.hpp>

class PutLayerModelToFront
{
    public:
        PutLayerModelToFront(
                Path<SlotModel>&& slotPath,
                const Id<LayerModel>& pid);

        void redo() const;

    private:
        Path<SlotModel> m_slotPath;
        const Id<LayerModel>& m_pid;
};
