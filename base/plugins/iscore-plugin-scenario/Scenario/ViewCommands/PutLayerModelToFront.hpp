#pragma once
#include <iscore/tools/ModelPath.hpp>

namespace Process { class LayerModel; }
class SlotModel;
#include <iscore/tools/SettableIdentifier.hpp>

class PutLayerModelToFront
{
    public:
        PutLayerModelToFront(
                Path<SlotModel>&& slotPath,
                const Id<Process::LayerModel>& pid);

        void redo() const;

    private:
        Path<SlotModel> m_slotPath;
        const Id<Process::LayerModel>& m_pid;
};
