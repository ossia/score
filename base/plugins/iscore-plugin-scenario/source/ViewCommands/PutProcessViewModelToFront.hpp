#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
class ProcessViewModel;

class PutProcessViewModelToFront
{
    public:
        PutProcessViewModelToFront(ObjectPath&& deckPath,
                                   const id_type<ProcessViewModel>& pid);

        void redo();

    private:
        ObjectPath m_deckPath;
        const id_type<ProcessViewModel>& m_pid;
};
