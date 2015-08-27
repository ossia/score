#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
class LayerModel;
class SlotModel;

class PutLayerModelToFront
{
    public:
        PutLayerModelToFront(
                ModelPath<SlotModel>&& slotPath,
                const id_type<LayerModel>& pid);

        void redo();

    private:
        ModelPath<SlotModel> m_slotPath;
        const id_type<LayerModel>& m_pid;
};
