#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
class LayerModel;

class PutLayerModelToFront
{
    public:
        PutLayerModelToFront(ObjectPath&& slotPath,
                                   const id_type<LayerModel>& pid);

        void redo();

    private:
        ObjectPath m_slotPath;
        const id_type<LayerModel>& m_pid;
};
