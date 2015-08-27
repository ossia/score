#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
class LayerModel;
class SlotModel;

class PutLayerModelToFront
{
    public:
        PutLayerModelToFront(
                Path<SlotModel>&& slotPath,
                const Id<LayerModel>& pid);

        void redo();

    private:
        Path<SlotModel> m_slotPath;
        const Id<LayerModel>& m_pid;
};
