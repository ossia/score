#pragma once
#include <Repartition/session/MasterSession.hpp>

#include "NetworkDocumentPlugin.hpp"

namespace iscore {
struct DocumentContext;
}  // namespace iscore

namespace Network
{

class MasterNetworkPolicy : public NetworkPolicyInterface
{
    public:
        MasterNetworkPolicy(MasterSession* s,
                            const iscore::DocumentContext& c);

        MasterSession* session() const override
        { return m_session; }

    private:
        MasterSession* m_session{};
};
}
