#pragma once
#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
class MasterNetworkPolicy : public NetworkPluginPolicy
{
    public:
        MasterNetworkPolicy(MasterSession* s,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker);

        MasterSession* session() const override
        { return m_session; }

    private:
        MasterSession* m_session{};
};
